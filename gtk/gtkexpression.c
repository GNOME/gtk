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

typedef struct _GtkExpressionClass GtkExpressionClass;

struct _GtkExpression
{
  const GtkExpressionClass *expression_class;
  GType value_type;

  GtkExpression *owner;
};

struct _GtkExpressionWatch
{
  GtkExpression *expression;
  GtkExpressionNotify notify;
  gpointer user_data;
  GDestroyNotify user_destroy;
};

struct _GtkExpressionClass
{
  gsize struct_size;
  const char *type_name;

  void                  (* finalize)            (GtkExpression          *expr);
  gboolean              (* is_static)           (GtkExpression          *expr);
  gboolean              (* evaluate)            (GtkExpression          *expr,
                                                 gpointer                this,
                                                 GValue                 *value);

  gsize watch_size;
  void                  (* watch)               (GtkExpression          *self,
                                                 gpointer                this_,
                                                 GtkExpressionWatch     *watch);
  void                  (* unwatch)             (GtkExpression          *self,
                                                 GtkExpressionWatch     *watch);
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

static void
gtk_expression_watch_static (GtkExpression      *self,
                             gpointer            this_,
                             GtkExpressionWatch *watch)
{
}

static void
gtk_expression_unwatch_static (GtkExpression      *self,
                               GtkExpressionWatch *watch)
{
}

static void
gtk_expression_watch_notify (GtkExpressionWatch *watch)
{
  watch->notify (watch->user_data);
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
gtk_constant_expression_is_static (GtkExpression *expr)
{
  return TRUE;
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
  gtk_constant_expression_is_static,
  gtk_constant_expression_evaluate,
  sizeof (GtkExpressionWatch),
  gtk_expression_watch_static,
  gtk_expression_unwatch_static
};

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
       * in a sane state if an error condition occoured
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

static gboolean
gtk_property_expression_is_static (GtkExpression *expr)
{
  return FALSE;
}

static GObject *
gtk_property_expression_get_object (GtkPropertyExpression *self,
                                    gpointer               this)
{
  GValue expr_value = G_VALUE_INIT;
  GObject *object;

  if (self->expr == NULL)
    return g_object_ref (this);

  if (!gtk_expression_evaluate (self->expr, this, &expr_value))
    return NULL;

  if (!G_VALUE_HOLDS_OBJECT (&expr_value))
    {
      g_value_unset (&expr_value);
      return NULL;
    }

  object = g_value_dup_object (&expr_value);
  g_value_unset (&expr_value);
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

  if (!G_TYPE_CHECK_INSTANCE_TYPE (object, self->pspec->owner_type))
    {
      g_object_unref (object);
      return FALSE;
    }

  g_object_get_property (object, self->pspec->name, value);
  g_object_unref (object);
  return TRUE;
}

typedef struct _GtkPropertyExpressionWatch GtkPropertyExpressionWatch;
struct _GtkPropertyExpressionWatch
{
  GtkExpressionWatch watch;

  GClosure *closure;
  GObject *this;
  GtkExpressionWatch *expr_watch; 
};

static void
gtk_property_expression_watch_weak_ref_cb (gpointer  data,
                                           GObject  *object)
{
  GtkPropertyExpressionWatch *pwatch = data;

  pwatch->this = NULL;

  gtk_expression_watch_notify (data);
}

static void
gtk_property_expression_watch_destroy_closure (GtkPropertyExpressionWatch *pwatch)
{
  if (pwatch->closure == NULL)
    return;

  g_closure_invalidate (pwatch->closure);
  g_closure_unref (pwatch->closure);
  pwatch->closure = NULL;
}

static void
gtk_property_expression_watch_create_closure (GtkPropertyExpressionWatch *pwatch)
{
  GtkExpressionWatch *watch = (GtkExpressionWatch *) pwatch;
  GtkPropertyExpression *self = (GtkPropertyExpression *) watch->expression;
  GObject *object;

  if (pwatch->this == NULL)
    return;
  object = gtk_property_expression_get_object (self, pwatch->this);
  if (object == NULL)
    return;

  pwatch->closure = g_cclosure_new_swap (G_CALLBACK (gtk_expression_watch_notify), pwatch, NULL);
  if (!g_signal_connect_closure_by_id (object,
                                       g_signal_lookup ("notify", G_OBJECT_TYPE (object)),
                                       g_quark_from_string (self->pspec->name),
                                       g_closure_ref (pwatch->closure),
                                       FALSE))
    {
      g_assert_not_reached ();
    }

  g_object_unref (object);
}

static void
gtk_property_expression_watch_expr_notify_cb (gpointer data)
{
  GtkPropertyExpressionWatch *pwatch = data;

  gtk_property_expression_watch_destroy_closure (pwatch);
  gtk_property_expression_watch_create_closure (pwatch);
  gtk_expression_watch_notify (data);
}

static void
gtk_property_expression_watch (GtkExpression      *expr,
                               gpointer            this,
                               GtkExpressionWatch *watch)
{
  GtkPropertyExpressionWatch *pwatch = (GtkPropertyExpressionWatch *) watch;
  GtkPropertyExpression *self = (GtkPropertyExpression *) expr;

  pwatch->this = this;
  g_object_weak_ref (this, gtk_property_expression_watch_weak_ref_cb, pwatch);

  if (self->expr && !gtk_expression_is_static (self->expr))
    {
      pwatch->expr_watch = gtk_expression_watch (self->expr,
                                                 this,
                                                 gtk_property_expression_watch_expr_notify_cb,
                                                 pwatch,
                                                 NULL);
    }

  gtk_property_expression_watch_create_closure (pwatch);
}

static void
gtk_property_expression_unwatch (GtkExpression      *expr,
                                 GtkExpressionWatch *watch)
{
  GtkPropertyExpressionWatch *pwatch = (GtkPropertyExpressionWatch *) watch;

  gtk_property_expression_watch_destroy_closure (pwatch);

  g_clear_pointer (&pwatch->expr_watch, gtk_expression_watch_unwatch);
  if (pwatch->this)
    g_object_weak_unref (pwatch->this, gtk_property_expression_watch_weak_ref_cb, pwatch);
}

static const GtkExpressionClass GTK_PROPERTY_EXPRESSION_CLASS =
{
  sizeof (GtkPropertyExpression),
  "GtkPropertyExpression",
  gtk_property_expression_finalize,
  gtk_property_expression_is_static,
  gtk_property_expression_evaluate,
  sizeof (GtkPropertyExpressionWatch),
  gtk_property_expression_watch,
  gtk_property_expression_unwatch
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
gtk_closure_expression_is_static (GtkExpression *expr)
{
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guint i;

  for (i = 0; i < self->n_params; i++)
    {
      if (!gtk_expression_is_static (self->params[i]))
        return FALSE;
    }

  return TRUE;
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
  g_value_init_from_instance (instance_and_params, this);

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

typedef struct _GtkClosureExpressionWatch GtkClosureExpressionWatch;
struct _GtkClosureExpressionWatch
{
  GtkExpressionWatch watch;

  GSList *watches;
};

static void
gtk_closure_expression_watch (GtkExpression      *expr,
                              gpointer            this_,
                              GtkExpressionWatch *watch)
{
  GtkClosureExpressionWatch *cwatch = (GtkClosureExpressionWatch *) watch;
  GtkClosureExpression *self = (GtkClosureExpression *) expr;
  guint i;

  for (i = 0; i < self->n_params; i++)
    {
      if (gtk_expression_is_static (self->params[i]))
        continue;

      cwatch->watches = g_slist_prepend (cwatch->watches,
                                         gtk_expression_watch (self->params[i],
                                                               this_,
                                                               (GtkExpressionNotify) gtk_expression_watch_notify,
                                                               watch,
                                                               NULL));
    }
}

static void
gtk_closure_expression_unwatch (GtkExpression      *expr,
                                GtkExpressionWatch *watch)
{
  GtkClosureExpressionWatch *cwatch = (GtkClosureExpressionWatch *) watch;

  g_slist_free_full (cwatch->watches, (GDestroyNotify) gtk_expression_watch_unwatch);
}

static const GtkExpressionClass GTK_CLOSURE_EXPRESSION_CLASS =
{
  sizeof (GtkClosureExpression),
  "GtkClosureExpression",
  gtk_closure_expression_finalize,
  gtk_closure_expression_is_static,
  gtk_closure_expression_evaluate,
  sizeof (GtkClosureExpressionWatch),
  gtk_closure_expression_watch,
  gtk_closure_expression_unwatch
};

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
    result->params[i] = gtk_expression_ref (params[i]);

  return (GtkExpression *) result;
}

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
 * @this_: (transfer none) (type GObject): the this argument for the evaluation
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
  g_return_val_if_fail (value != NULL, FALSE);

  return self->expression_class->evaluate (self, this_, value);
}

/**
 * gtk_expression_is_static:
 * @self: a #GtkExpression
 *
 * Checks if the expression is static.
 *
 * A static expression will never change its result when 
 * gtk_expression_evaluate() is called on it with the same arguments.
 *
 * That means a call to gtk_expression_watch() is not necessary because
 * it will never trigger a notify.
 *
 * Returns: %TRUE if the expression is static
 **/
gboolean
gtk_expression_is_static (GtkExpression *self)
{
  return self->expression_class->is_static (self);
}

/**
 * gtk_expression_watch:
 * @self: a #GtkExpression
 * @this_: (transfer none) (type GObject): the this argument to
 *     watch
 * @notify: (closure user_data): callback to invoke when the
 *     expression changes
 * @user_data: user data to pass to @notify callback
 * @user_destroy: destroy notify for @user_data
 *
 * Installs a watch for the given @expression that calls the @notify function
 * whenever the evaluation of @self may have changed.
 *
 * GTK cannot guarantee that the evaluation did indeed change when the @notify
 * gets invoked, but it guarantees the opposite: When it did in fact change,
 * the @notify will be invoked.
 *
 * Returns: (transfer full) the newly installed watch
 **/
GtkExpressionWatch *
gtk_expression_watch (GtkExpression       *self,
                      gpointer             this_,
                      GtkExpressionNotify  notify,
                      gpointer             user_data,
                      GDestroyNotify       user_destroy)
{
  GtkExpressionWatch *watch;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (notify != NULL, NULL);

  watch = g_atomic_rc_box_alloc0 (self->expression_class->watch_size);

  watch->expression = gtk_expression_ref (self);
  watch->notify = notify;
  watch->user_data = user_data;
  watch->user_destroy = user_destroy;

  self->expression_class->watch (self, this_, watch);

  return watch;
}

static void
gtk_expression_watch_finalize (gpointer data)
{
  GtkExpressionWatch *watch = data;

  watch->expression->expression_class->unwatch (watch->expression, watch);

  if (watch->user_destroy)
    watch->user_destroy (watch->user_data);
}

/**
 * gtk_expression_watch_unwatch:
 * @watch: (transfer full) watch to release
 *
 * Stops watching an expression that was established via gtk_expression_watch().
 **/
void
gtk_expression_watch_unwatch (GtkExpressionWatch *watch)
{
  g_atomic_rc_box_release_full (watch, gtk_expression_watch_finalize);
}
