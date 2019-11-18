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

#include "gtkexpression.h"

#include <gobject/gvaluecollector.h>

/**
 * SECTION:gtkexpression
 * @Short_description: Expressions to values
 * @Title: GtkExpression
 *
 * GtkExpression provides a way to describe references to #GValues.
 *
 * An expression needs to be `evaluated` to obtain the value that it currently refers
 * to. An evaluation always happens in the context of a current object called `this`
 * (it mirrors the behavior of object-oriented languages), which may or may not
 * influence the result of the evaluation. Use gtk_expression_evaluate() for
 * evaluating an expression.
 *
 * Various methods for defining expressions exist, from simple constants via
 * gtk_constant_expression_new() to looking up properties in a #GObject (even
 * recursively) via gtk_property_expression_new() or providing custom functions to
 * transform and combine expressions via gtk_closure_expression_new().
 *
 * By default, expressions are not paying attention to changes and evaluation is
 * just a snapshot of the current state at a given time. To get informed about
 * changes, an expression needs to be `watched` via a #GtkExpressionWatch, which
 * will cause a callback to be called whenever the value of the expression may
 * have changed. gtk_expression_watch() starts watching an expression, and
 * gtk_expression_watch_unwatch() stops.
 *
 * Watches can be created for automatically updating the propery of an object,
 * similar to GObject's #GBinding mechanism, by using gtk_expression_bind().
 *
 * #GtkExpression in ui files
 *
 * GtkBuilder has support for creating expressions. The syntax here can be used where
 * a #GtkExpression object is needed like in a <property> tag for an expression
 * property, or in a <binding> tag to bind a property to an expression.
 *
 * To create an property expression, use the <lookup> element. It can have a `type`
 * attribute to specify the object type, and a `name` attribute to specify the property
 * to look up. The content of <lookup> can either be an element specfiying the expression
 * to use the object, or a string that specifies the name of the object to use.
 * 
 * Example:
 * |[
 *   <lookup name='search'>string_filter</lookup>
 * |]
 *
 * To create a constant expression, use the <constant> element. If the type attribute
 * is specified, the element content is interpreted as a value of that type. Otherwise,
 * it is assumed to be an object.
 *
 * Example:
 * |[
 *   <constant>string_filter</constant>
 *   <constant type='gchararray'>Hello, world</constant>
 * ]|
 *
 * To create a closure expression, use the <closure> element. The `type` and `function`
 * attributes specify what function to use for the closure, the content of the element
 * contains the expressions for the parameters.
 * 
 * Example:
 * |[
 *   <closure type='gchararray' function='combine_args_somehow'>
 *     <constant type='gchararray'>File size:</constant>
 *     <lookup type='GFile' name='size'>myfile</lookup>
 *   </closure>
 * ]|
 */
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
  gboolean              (* evaluate)            (GtkExpression          *expr,
                                                 gpointer                this,
                                                 GValue                 *value);
};

/**
 * GtkExpression: (ref-func gtk_expression_ref) (unref-func gtk_expression_unref)
 *
 * The `GtkExpression` structure contains only private data.
 */

G_DEFINE_BOXED_TYPE (GtkExpression, gtk_expression,
                     gtk_expression_ref,
                     gtk_expression_unref)

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

/*** CONSTANT ***/

typedef struct _GtkConstantExpression GtkConstantExpression;

struct _GtkConstantExpression
{
  GtkExpression parent;

  GValue value;
};

static void
gtk_constant_expression_finalize (GtkExpression *expr)
{
  GtkConstantExpression *self = (GtkConstantExpression *) expr;

  g_value_unset (&self->value);
}

static gboolean
gtk_constant_expression_evaluate (GtkExpression *expr,
                                  gpointer       this,
                                  GValue        *value)
{
  GtkConstantExpression *self = (GtkConstantExpression *) expr;

  g_value_init (value, G_VALUE_TYPE (&self->value));
  g_value_copy (&self->value, value);
  return TRUE;
}

static const GtkExpressionClass GTK_CONSTANT_EXPRESSION_CLASS =
{
  sizeof (GtkConstantExpression),
  "GtkConstantExpression",
  gtk_constant_expression_finalize,
  gtk_constant_expression_evaluate,
};

/**
 * gtk_constant_expression_new:
 * @value_type: The type of the object
 * @...: arguments to create the object from
 *
 * Creates a GtkExpression that evaluates to the
 * object given by the arguments.
 *
 * Returns: a new #GtkExpression
 */
GtkExpression *
gtk_constant_expression_new (GType value_type,
                             ...)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *result;
  va_list args;
  char *error;

  va_start (args, value_type);
  G_VALUE_COLLECT_INIT (&value, value_type,
                        args, G_VALUE_NOCOPY_CONTENTS,
                        &error);
  if (error)
    {
      g_critical ("%s: %s", G_STRLOC, error);
      g_free (error);
      /* we purposely leak the value here, it might not be
       * in a sane state if an error condition occurred
       */
      return NULL;
    }

  result = gtk_constant_expression_new_for_value (&value);

  g_value_unset (&value);
  va_end (args);

  return result;
}

/**
 * gtk_constant_expression_new_for_value:
 * @value: a #GValue
 *
 * Creates an expression that always evaluates to the given @value.
 *
 * Returns: a new #GtkExpression
 **/
GtkExpression *
gtk_constant_expression_new_for_value (const GValue *value)
{
  GtkConstantExpression *result;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  result = gtk_expression_alloc (&GTK_CONSTANT_EXPRESSION_CLASS, G_VALUE_TYPE (value));

  g_value_init (&result->value, G_VALUE_TYPE (value));
  g_value_copy (value, &result->value);

  return (GtkExpression *) result;
}

/*** OBJECT ***/

typedef struct _GtkObjectExpression GtkObjectExpression;

struct _GtkObjectExpression
{
  GtkExpression parent;

  GObject *object;
};

static void
gtk_object_expression_weak_ref_cb (gpointer  data,
                                   GObject  *object)
{
  GtkObjectExpression *self = (GtkObjectExpression *) data;

  self->object = NULL;
}

static void
gtk_object_expression_finalize (GtkExpression *expr)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  if (self->object)
    g_object_weak_unref (self->object, gtk_object_expression_weak_ref_cb, self);
}

static gboolean
gtk_object_expression_evaluate (GtkExpression *expr,
                                gpointer       this,
                                GValue        *value)
{
  GtkObjectExpression *self = (GtkObjectExpression *) expr;

  if (self->object == NULL)
    return FALSE;

  g_value_init (value, gtk_expression_get_value_type (expr));
  g_value_set_object (value, self->object);
  return TRUE;
}

static const GtkExpressionClass GTK_OBJECT_EXPRESSION_CLASS =
{
  sizeof (GtkObjectExpression),
  "GtkObjectExpression",
  gtk_object_expression_finalize,
  gtk_object_expression_evaluate
};

/**
 * gtk_object_expression_new:
 * @object: (transfer none): object to watch
 *
 * Creates an expression evaluating to the given @object with a weak reference.
 * Once the @object is disposed, it will fail to evaluate.
 * This expression is meant to break reference cycles.
 *
 * If you want to keep a reference to @object, use gtk_constant_expression_new().
 *
 * Returns: a new #GtkExpression
 **/
GtkExpression *
gtk_object_expression_new (GObject *object)
{
  GtkObjectExpression *result;

  g_return_val_if_fail (G_IS_OBJECT (object), NULL);

  result = gtk_expression_alloc (&GTK_OBJECT_EXPRESSION_CLASS, G_OBJECT_TYPE (object));

  result->object = object;
  g_object_weak_ref (object, gtk_object_expression_weak_ref_cb, result);

  return (GtkExpression *) result;
}

/*** PROPERTY ***/

typedef struct _GtkPropertyExpression GtkPropertyExpression;

struct _GtkPropertyExpression
{
  GtkExpression parent;

  GtkExpression *expr;

  GParamSpec *pspec;
};

static void
gtk_property_expression_finalize (GtkExpression *expr)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  g_clear_pointer (&self->expr, gtk_expression_unref);
}

static GObject *
gtk_property_expression_get_object (GtkPropertyExpression *self,
                                    gpointer               this)
{
  GValue expr_value = G_VALUE_INIT;
  GObject *object;

  if (self->expr == NULL)
    {
      if (this)
        return g_object_ref (this);
      else
        return NULL;
    }

  if (!gtk_expression_evaluate (self->expr, this, &expr_value))
    return NULL;

  if (!G_VALUE_HOLDS_OBJECT (&expr_value))
    {
      g_value_unset (&expr_value);
      return NULL;
    }

  object = g_value_dup_object (&expr_value);
  g_value_unset (&expr_value);

  if (!G_TYPE_CHECK_INSTANCE_TYPE (object, self->pspec->owner_type))
    {
      g_object_unref (object);
      return NULL;
    }

  return object;
}

static gboolean
gtk_property_expression_evaluate (GtkExpression *expr,
                                  gpointer       this,
                                  GValue        *value)
{
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;
  GObject *object;

  object = gtk_property_expression_get_object (self, this);
  if (object == NULL)
    return FALSE;

  g_object_get_property (object, self->pspec->name, value);
  g_object_unref (object);
  return TRUE;
}

static const GtkExpressionClass GTK_PROPERTY_EXPRESSION_CLASS =
{
  sizeof (GtkPropertyExpression),
  "GtkPropertyExpression",
  gtk_property_expression_finalize,
  gtk_property_expression_evaluate,
};

/**
 * gtk_property_expression_new:
 * @this_type: The type to expect for the this type
 * @expression: (nullable) (transfer full): Expression to
 *     evaluate to get the object to query or %NULL to
 *     query the `this` object
 * @property_name: name of the property
 *
 * Creates an expression that looks up a property via the
 * given @expression or the `this` argument when @expression
 * is %NULL.
 *
 * If the resulting object conforms to @this_type, its property
 * named @property_name will be queried.
 * Otherwise, this expression's evaluation will fail.
 *
 * The given @this_type must have a property with @property_name.  
 *
 * Returns: a new #GtkExpression
 **/
GtkExpression *
gtk_property_expression_new (GType          this_type,
                             GtkExpression *expression,
                             const char    *property_name)
{
  GtkPropertyExpression *result;
  GParamSpec *pspec;

  if (g_type_is_a (this_type, G_TYPE_OBJECT))
    {
      pspec = g_object_class_find_property (g_type_class_peek (this_type), property_name);
    }
  else if (g_type_is_a (this_type, G_TYPE_INTERFACE))
    {
      pspec = g_object_interface_find_property (g_type_default_interface_peek (this_type), property_name);
    }
  else
    {
      g_critical ("Type `%s` does not support properties", g_type_name (this_type));
      return NULL;
    }

  if (pspec == NULL)
    {
      g_critical ("Type `%s` does not have a property named `%s`", g_type_name (this_type), property_name);
      return NULL;
    }

  result = gtk_expression_alloc (&GTK_PROPERTY_EXPRESSION_CLASS, pspec->value_type);

  result->pspec = pspec;
  result->expr = expression;

  return (GtkExpression *) result;
}

/*** CLOSURE ***/

typedef struct _GtkClosureExpression GtkClosureExpression;

struct _GtkClosureExpression
{
  GtkExpression parent;

  GClosure *closure;
  guint n_params;
  GtkExpression **params;
};

static void
gtk_closure_expression_finalize (GtkExpression *expr)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guint i;

  for (i = 0; i < self->n_params; i++)
    {
      gtk_expression_unref (self->params[i]);
    }
  g_free (self->params);

  g_closure_unref (self->closure);
}

static gboolean
gtk_closure_expression_evaluate (GtkExpression *expr,
                                 gpointer       this,
                                 GValue        *value)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  GValue *instance_and_params;
  gboolean result = TRUE;
  guint i;

  instance_and_params = g_alloca (sizeof (GValue) * (self->n_params + 1));
  memset (instance_and_params, 0, sizeof (GValue) * (self->n_params + 1));

  for (i = 0; i < self->n_params; i++)
    {
      if (!gtk_expression_evaluate (self->params[i], this, &instance_and_params[i + 1]))
        {
          result = FALSE;
          goto out;
        }
    }
  if (this)
    g_value_init_from_instance (instance_and_params, this);
  else
    g_value_init (instance_and_params, G_TYPE_OBJECT);

  g_value_init (value, expr->value_type);
  g_closure_invoke (self->closure,
                    value,
                    self->n_params + 1,
                    instance_and_params,
                    NULL);

out:
  for (i = 0; i < self->n_params + 1; i++)
    g_value_unset (&instance_and_params[i]);

  return result;
}

static const GtkExpressionClass GTK_CLOSURE_EXPRESSION_CLASS =
{
  sizeof (GtkClosureExpression),
  "GtkClosureExpression",
  gtk_closure_expression_finalize,
  gtk_closure_expression_evaluate,
};

/**
 * gtk_closure_expression_new:
 * @type: the type of the value that this expression evaluates to
 * @closure: closure to call when evaluating this expression. If closure is floating, it is adopted
 * @n_params: the number of params needed for evaluating @closure
 * @params: (array length=n_params) (transfer full): expressions for each parameter
 *
 * Creates a GtkExpression that calls @closure when it is evaluated.
 * @closure is called with the @this object and the results of evaluating
 * the @params expressions.
 *
 * Returns: a new #GtkExpression
 */
GtkExpression *
gtk_closure_expression_new (GType                value_type,
                            GClosure            *closure,
                            guint                n_params,
                            GtkExpression      **params)
{
  GtkClosureExpression *result;
  guint i;

  g_return_val_if_fail (closure != NULL, NULL);
  g_return_val_if_fail (n_params == 0 || params != NULL, NULL);

  result = gtk_expression_alloc (&GTK_CLOSURE_EXPRESSION_CLASS, value_type);

  result->closure = g_closure_ref (closure);
  g_closure_sink (closure);
  if (G_CLOSURE_NEEDS_MARSHAL (closure))
    g_closure_set_marshal (closure, g_cclosure_marshal_generic);

  result->n_params = n_params;
  result->params = g_new (GtkExpression *, n_params);
  for (i = 0; i < n_params; i++)
    result->params[i] = params[i];

  return (GtkExpression *) result;
}

/**
 * gtk_cclosure_expression_new:
 * @type: the type of the value that this expression evaluates to
 * @marshal: marshaller used for creating a closure
 * @n_params: the number of params needed for evaluating @closure
 * @params: (array length=n_params) (transfer full): expressions for each parameter
 * @callback_func: callback used for creating a closure
 * @user_data: user data used for creating a closure
 * @user_destroy: destroy notify for @user_data
 *
 * This function is a variant of gtk_closure_expression_new() that
 * creates a #GClosure by calling gtk_cclosure_new() with the given
 * @callback_func, @user_data and @user_destroy.
 *
 * Returns: a new #GtkExpression
 */
GtkExpression *
gtk_cclosure_expression_new (GType                value_type,
                             GClosureMarshal      marshal,
                             guint                n_params,
                             GtkExpression      **params,
                             GCallback            callback_func,
                             gpointer             user_data,
                             GClosureNotify       user_destroy)
{
  GClosure *closure;

  closure = g_cclosure_new (callback_func, user_data, user_destroy);
  if (marshal)
    g_closure_set_marshal (closure, marshal);

  return gtk_closure_expression_new (value_type, closure, n_params, params);
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
 * @this_: (transfer none) (type GObject) (nullable): the this argument for the evaluation
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
                         gpointer       this_,
                         GValue        *value)
{
  g_return_val_if_fail (GTK_IS_EXPRESSION (self), FALSE);
  g_return_val_if_fail (this_ == NULL || G_IS_OBJECT (this_), FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  return self->expression_class->evaluate (self, this_, value);
}

