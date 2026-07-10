/*
 * Copyright © Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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

#include "gtkbuilder.h"
#include "gtkstatenodeprivate.h"
#include "gtkrestorableprivate.h"

#include <gobject/gvaluecollector.h>

/**
 * GtkStateNode:
 *
 * Implementation of [iface@Gtk.Restorable] that allows apps to construct their
 * state tree out of bindings to object properties, other `GtkRestorable`
 * objects, or constant data.
 *
 * Rather than implementing `GtkRestorable` manually, objects could construct
 * a `GtkStateNode`, declare the state they want in the tree via bindings,
 * and then delegate their implementation of `GtkRestorable`. See the
 * documentation of `GtkRestorable` for details.
 *
 * ## GtkStateNode in .ui files
 *
 * `GtkBuilder` has support for `GtkStateNode`. This further simplifies the
 * process of setting up the state tree, by allowing apps to create bindings
 * declaratively.
 *
 * Inside of an `<object class="GtkStateNode">`, `GtkBuilder` permits `<state>`
 * elements, which declare `GtkStateNode` bindings. Each `<state>` binding
 * requires a `key` attribute to specify the name of the binding, and then
 * as a value takes the syntax for constructing a `GtkExpression` (similar to
 * the `<binding>` element).
 *
 * Flags are exposed as optional boolean attributes on the `<state>` element.
 * `save-only` controls `GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY`.
 * `restore-only` controls `GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY`.
 * `restore-always` controls `GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS`.
 *
 * For example:
 *
 * ```xml
 * <object class="GtkStateNode">
 *   <state key="some_state">
 *     <lookup name="some_property">some_object</lookup>
 *   </state>
 *   <state key="some_legacy_state" restore-only="true">
 *     <lookup name="some_other_property">
 *       <lookup name="some_inner_object">some_object</lookup>
 *     </lookup>
 *   </state>
 * </object>
 * ```
 */

struct _GtkStateNode {
  GObject parent_instance;

  GHashTable *bindings; /* string => GtkStateNodeBinding */
  gboolean bindings_dirty;
};

static void gtk_state_node_restorable_iface_init (GtkRestorableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (GtkStateNode, gtk_state_node, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (GTK_TYPE_RESTORABLE,
                                                      gtk_state_node_restorable_iface_init))

typedef struct {
  GtkExpression *expression;
  GtkExpressionWatch *watch;
  GtkStateNodeBindingFlags flags;
} GtkStateNodeBinding;

static void
gtk_state_node_binding_free (GtkStateNodeBinding *self)
{
  gtk_expression_unref (self->expression);

  if (self->watch)
    {
      gtk_expression_watch_unwatch (self->watch);
      gtk_expression_watch_unref (self->watch);
    }

  g_free (self);
}

typedef struct {
  GTask *task;

  GCancellable *cancellable;
  gulong cancellable_chain_handler;

  gulong pending;
  gboolean returned;
} GtkStateNodeTaskContext;

static void
gtk_state_node_task_context_free (GtkStateNodeTaskContext *self)
{
  g_assert (self->returned);

  g_cancellable_disconnect (g_task_get_cancellable (self->task),
                            self->cancellable_chain_handler);
  g_object_unref (self->cancellable);
  g_object_unref (self->task);

  g_free (self);
}

static void
chain_cancellable (GCancellable *source,
                   gpointer      user_data)
{
  GCancellable *target = G_CANCELLABLE (user_data);
  g_cancellable_cancel (target);
}

static GtkStateNodeTaskContext *
gtk_state_node_task_context_init (GTask *task)
{
  GtkStateNodeTaskContext *self;

  self = g_new0 (GtkStateNodeTaskContext, 1);
  self->task = g_object_ref (task);
  self->cancellable = g_cancellable_new ();
  self->cancellable_chain_handler = g_cancellable_connect (g_task_get_cancellable (task),
                                                           G_CALLBACK (chain_cancellable),
                                                           g_object_ref (self->cancellable),
                                                           g_object_unref);
  self->pending = 1;

  g_task_set_task_data (task, self,
                        (GDestroyNotify) gtk_state_node_task_context_free);
  return self;
}

static void
gtk_state_node_task_context_return (GTask    *task,
                                    gboolean  success,
                                    GError   *error)
{
  GtkStateNodeTaskContext *self = g_task_get_task_data (task);

  self->pending--;

  if (success)
    {
      if (self->pending == 0 && !self->returned)
        {
          g_task_return_boolean (self->task, TRUE);
          self->returned = TRUE;
        }
    }
  else
    {
      if (!self->returned)
        {
          g_task_return_error (self->task, g_steal_pointer (&error));
          self->returned = TRUE;
        }

      g_cancellable_cancel (self->cancellable); /* Cancel remaining ongoing work */
      g_clear_error (&error);
    }
}

static void
save_restorable_cb (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GtkRestorable *self = GTK_RESTORABLE (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  gboolean success;

  success = gtk_restorable_save_state_finish (self, result, &error);
  gtk_state_node_task_context_return (task, success, g_steal_pointer (&error));
  g_clear_object (&task);
}

static void
save_restorable (GtkRestorable  *restorable,
                 GtkSaveContext *context,
                 GTask          *task)
{
  GtkStateNodeTaskContext *task_ctx = g_task_get_task_data (task);

  task_ctx->pending++;
  gtk_restorable_save_state (restorable,
                             context,
                             task_ctx->cancellable,
                             save_restorable_cb,
                             g_object_ref (task));
}

static void
save_expression (GtkExpression  *expression,
                 GtkSaveContext *context,
                 const char     *key,
                 GTask          *task)
{
  GValue value = G_VALUE_INIT;

  if (!gtk_expression_evaluate (expression, NULL, &value))
    return;

  if (g_type_is_a (G_VALUE_TYPE (&value), GTK_TYPE_RESTORABLE))
    {
      GtkRestorable *restorable = GTK_RESTORABLE (g_value_get_object (&value));
      GtkSaveContext *child_context = gtk_save_context_child (context, key);
      save_restorable (restorable, context, task);
      gtk_save_context_unref (child_context);
    }
  else
    {
      GValue transformed = G_VALUE_INIT;
      g_value_init (&transformed, G_TYPE_VARIANT);

      if (!g_value_transform (&value, &transformed))
        g_assert_not_reached ();

      gtk_save_context_insert_value (context, key,
                                     g_value_get_variant (&transformed));
      g_value_unset (&transformed);
    }

  g_value_unset (&value);
}

static void
save_state_async (GtkRestorable       *restorable,
                  GtkSaveContext      *context,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  GtkStateNode *self = GTK_STATE_NODE (restorable);
  GTask *task;
  GHashTableIter iter;
  const char *key;
  GtkStateNodeBinding *binding;

  task = g_task_new (restorable, cancellable, callback, user_data);
  g_task_set_source_tag (task, save_state_async);
  gtk_state_node_task_context_init (task);

  self->bindings_dirty = FALSE;

  g_hash_table_iter_init (&iter, self->bindings);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &binding))
    {
      if (binding->flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY)
        continue;
      save_expression (binding->expression, context, key, task);
    }

  gtk_state_node_task_context_return (task, TRUE, NULL);
}

static gboolean
save_state_finish (GtkRestorable  *restorable,
                   GAsyncResult   *result,
                   GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, restorable), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
restore_restorable_cb (GObject      *source_object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GtkRestorable *self = GTK_RESTORABLE (source_object);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;
  gboolean success;

  success = gtk_restorable_restore_state_finish (self, result, &error);
  gtk_state_node_task_context_return (task, success, g_steal_pointer (&error));
  g_clear_object (&task);
}

static void
restore_restorable (GtkRestorable     *restorable,
                    GtkRestoreContext *context,
                    GTask             *task)
{
  GtkStateNodeTaskContext *task_ctx = g_task_get_task_data (task);

  task_ctx->pending++;
  gtk_restorable_restore_state (restorable,
                                context,
                                task_ctx->cancellable,
                                restore_restorable_cb,
                                g_object_ref (task));
}

static void
restore_expression (GtkExpression     *expression,
                    GtkRestoreContext *context,
                    const char        *key,
                    GTask             *task)
{
  if (g_type_is_a (gtk_expression_get_value_type (expression), GTK_TYPE_RESTORABLE))
    {
      GValue value = G_VALUE_INIT;
      GtkRestorable *restorable;
      GtkRestoreContext *child_context;

      if (!gtk_expression_evaluate (expression, NULL, &value))
        return;

      restorable = GTK_RESTORABLE (g_value_get_object (&value));
      child_context = gtk_restore_context_child (context, key);
      restore_restorable (restorable, child_context, task);
      gtk_restore_context_unref (child_context);
      g_value_unset (&value);
    }
  else if (GTK_IS_PROPERTY_EXPRESSION (expression))
    {
      GtkExpression *object_expr;
      GValue value = G_VALUE_INIT, variant_value = G_VALUE_INIT;
      GObject *object;
      GVariant *variant;
      GParamSpec *pspec;

      object_expr = gtk_property_expression_get_expression (expression);
      if (!gtk_expression_evaluate (object_expr, NULL, &value))
        return;
      object = g_value_dup_object (&value);
      g_value_unset (&value);

      variant = gtk_restore_context_lookup_value (context, key, NULL);
      if (!variant)
        return;
      g_value_init (&variant_value, G_TYPE_VARIANT);
      g_value_take_variant (&variant_value, g_steal_pointer (&variant));
      pspec = gtk_property_expression_get_pspec (expression);
      g_value_init (&value, pspec->value_type);
      if (!g_value_transform (&variant_value, &value))
        g_assert_not_reached ();
      g_value_unset (&variant_value);

      g_object_set_property (object, pspec->name, &value);
      g_value_unset (&value);
      g_object_unref (object);
    }
  else
    {
      g_assert_not_reached ();
    }
}

static void
restore_state_async (GtkRestorable       *restorable,
                     GtkRestoreContext   *context,
                     GCancellable        *cancellable,
                     GAsyncReadyCallback  callback,
                     gpointer             user_data)
{
  GtkStateNode *self = GTK_STATE_NODE (restorable);
  GTask *task;
  GHashTableIter iter;
  const char *key;
  GtkStateNodeBinding *binding;

  task = g_task_new (restorable, cancellable, callback, user_data);
  g_task_set_source_tag (task, restore_state_async);
  gtk_state_node_task_context_init (task);

  g_hash_table_iter_init (&iter, self->bindings);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &binding))
    {
      if (binding->flags & GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY)
        continue;

      if (!(binding->flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS) &&
          gtk_restore_context_get_reason (context) <= GTK_RESTORE_REASON_LAUNCH)
        {
          continue;
        }

      restore_expression (binding->expression, context, key, task);
    }

  gtk_state_node_task_context_return (task, TRUE, NULL);
}

static gboolean
restore_state_finish (GtkRestorable  *restorable,
                      GAsyncResult   *result,
                      GError        **error)
{
  g_return_val_if_fail (g_task_is_valid (result, restorable), FALSE);
  return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
check_state_dirty (GtkRestorable *restorable)
{
  GtkStateNode *self = GTK_STATE_NODE (restorable);
  GHashTableIter iter;
  const char *key;
  GtkStateNodeBinding *binding;

  /* True whenever the GtkExpression watch has fired. This handles the case where
   * we've bound a property and that property has changed, or the app has replaced
   * one of our GtkRestorables entirely! */
  if (self->bindings_dirty)
    return TRUE;

  /* Check all of our GtkRestorables and see if any have been marked dirty */
  g_hash_table_iter_init (&iter, self->bindings);
  while (g_hash_table_iter_next (&iter, (gpointer *) &key, (gpointer *) &binding))
    {
      GType binding_type;
      GValue value = G_VALUE_INIT;
      GtkRestorable *binding_restorable;
      gboolean binding_dirty;

      if (binding->flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY)
        continue;

      binding_type = gtk_expression_get_value_type (binding->expression);
      if (!g_type_is_a (binding_type, GTK_TYPE_RESTORABLE))
        continue;

      if (!gtk_expression_evaluate (binding->expression, NULL, &value))
        continue;

      binding_restorable = GTK_RESTORABLE (g_value_get_object (&value));
      binding_dirty = gtk_restorable_check_state_dirty (binding_restorable);
      g_value_unset (&value);

      if (binding_dirty)
        return TRUE;
    }

  return FALSE;
}

static void
gtk_state_node_restorable_iface_init (GtkRestorableInterface *iface)
{
  iface->save_state_async = save_state_async;
  iface->save_state_finish = save_state_finish;
  iface->restore_state_async = restore_state_async;
  iface->restore_state_finish = restore_state_finish;
  iface->check_state_dirty = check_state_dirty;
}

static void
gtk_state_node_class_init (GtkStateNodeClass *klass)
{
}

static void
gtk_state_node_init (GtkStateNode *self)
{
  self->bindings = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          (GDestroyNotify) gtk_state_node_binding_free);
}

/**
 * gtk_state_node_new: (constructor):
 *
 * Creates a new `GtkStateNode`.
 *
 * Returns: the new `GtkStateNode`
 */
GtkStateNode *
gtk_state_node_new (void)
{
  return g_object_new (GTK_TYPE_STATE_NODE, NULL);
}

static void
on_expression_watch (gpointer user_data)
{
  GtkStateNode *self = GTK_STATE_NODE (user_data);
  self->bindings_dirty = TRUE;
}

/*<private>
 * gtk_state_node_validate_expression:
 * @expr: the `GtkExpression` to check
 * @flags: the flags to check
 * @error: the `GError` to set if @expr or @flags are invalid
 *
 * Validates that the @expr and @flags are valid, and sets a descriptive
 * @error if not.
 *
 * Returns: %TRUE if the arguments are valid
 */
gboolean
gtk_state_node_validate_expression (GtkExpression             *expr,
                                    GtkStateNodeBindingFlags   flags,
                                    GError                   **error)
{
  GType value_type = gtk_expression_get_value_type (expr);
  gboolean can_save = !(flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY);
  gboolean can_restore = !(flags & GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY);

  if (!can_save && !can_restore)
    {
      g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
                   "State bindings cannot be save-only and restore-only simultaneously");
      return FALSE;
    }

  if (!can_restore && (flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS))
    {
      g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ATTRIBUTE,
                   "State bindings cannot be save-only and restore-always simultaneously");
      return FALSE;
    }

  if (g_type_is_a (value_type, GTK_TYPE_RESTORABLE))
    return TRUE;

  if (can_save && !g_value_type_transformable (value_type, G_TYPE_VARIANT))
    {
      g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                   "Type '%s' cannot be transformed to 'GVariant' for state binding. "
                   "You should register a 'GValueTransform' for serializing this type.",
                   g_type_name (value_type));
      return FALSE;
    }

  if (can_restore)
    {
      GParamSpec *pspec;

      if (!g_value_type_transformable (G_TYPE_VARIANT, value_type))
        {
          g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Type 'GVariant' cannot be transformed to '%s' for state binding. "
                       "You should register a 'GValueTransform' for deserializing this type.",
                       g_type_name (value_type));
          return FALSE;
        }

      if (!GTK_IS_PROPERTY_EXPRESSION (expr))
        {
          g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Cannot restore state to the provided expression. You may "
                       "need to set the save-only flag.");
          return FALSE;
        }

      pspec = gtk_property_expression_get_pspec (expr);
      if (pspec->flags & G_PARAM_CONSTRUCT_ONLY)
        {
          g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Cannot restore state to construct-only property. You may "
                       "need to set the save-only flag.");
          return FALSE;
        }
      else if (!(pspec->flags & G_PARAM_WRITABLE))
        {
          g_set_error (error, GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Cannot restore state to read-only property. You may "
                       "need to set the save-only flag.");
          return FALSE;
        }
    }

  return TRUE;
}

/**
 * gtk_state_node_bind_expression:
 * @self: a `GtkStateNode`
 * @key: the name of the binding
 * @source: a `GtkExpression` to bind
 * @flags: flags for this binding
 *
 * Binds @source as a child of @self, under @key.
 *
 * When collecting state, @source will be evaluated, the result will be serialized,
 * and then stored into the [struct@Gtk.SaveContext] under @key. The provided
 * `GtkExpression` must return a `GtkRestorable`, or a type that can be serialized
 * to a `GVariant` via [method@GObject.Value.transform]. You may need to register
 * a transformation function for your type via
 * [func@GObject.Value.register_transform_func].
 *
 * When restoring state, the state will be loaded from @key in the
 * [struct@Gtk.RestoreContext], deserialized, and then written back to @source.
 * The provided `GtkExpression` must either evaluate to a `GtkRestorable`, or
 * should be a `GtkPropertyExpression` that points at a writable property. If
 * your @source doesn't meet these criteria, you will need to pass
 * `GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY` to acknowledge that you intended to
 * create a save-only binding. This flag is implied whenever @source is a
 * `GtkConstantExpression`.
 *
 * By default, state is only restored to bindings when the restore reason is
 * greater than `GTK_RESTORE_REASON_LAUNCH` (i.e. not during normal app launches).
 * Bindings can opt into restoring during normal app launches by setting the
 * `GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ALWAYS` flag.
 *
 * The @source may be watched and @self will be marked dirty if the @source
 * changes. If the @source evaluates to a `GtkRestorable`, then @self will be
 * marked dirty whenever the `GtkRestorable` is marked dirty.
 */
void
gtk_state_node_bind_expression (GtkStateNode             *self,
                                const char               *key,
                                GtkExpression            *source,
                                GtkStateNodeBindingFlags  flags)
{
  GError *error = NULL;
  GtkStateNodeBinding *binding;

  g_return_if_fail (GTK_IS_STATE_NODE (self));
  g_return_if_fail (key && !g_hash_table_contains (self->bindings, key));
  g_return_if_fail (GTK_IS_EXPRESSION (source));

  if (GTK_IS_CONSTANT_EXPRESSION (source))
    flags |= GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY;

  if (!gtk_state_node_validate_expression (source, flags, &error))
    {
      g_critical ("%s", error->message);
      g_clear_error (&error);
      return;
    }

  binding = g_new0 (GtkStateNodeBinding, 1);
  binding->expression = gtk_expression_ref (source);
  binding->flags = flags;

  if (!(flags & GTK_STATE_NODE_BINDING_FLAGS_RESTORE_ONLY) &&
      !gtk_expression_is_static (source))
    {
      /* restore-only bindings can't ever be dirty and static expressions never
       * change, so we can skip the watch */
      binding->watch = gtk_expression_watch (source, NULL, on_expression_watch,
                                             self, NULL);
      gtk_expression_watch_ref (binding->watch);
    }

  g_hash_table_insert (self->bindings, g_strdup (key), binding);
}

/**
 * gtk_state_node_bind:
 * @self: a `GtkStateNode`
 * @key: the name of the binding
 * @source: a `GtkRestorable` to bind
 * @flags: flags for this binding
 *
 * Convenience method for binding a `GtkRestorable` to @self. Constructs
 * a `GtkConstantExpression` and calls [method@Gtk.StateNode.bind_expression].
 */
void
gtk_state_node_bind (GtkStateNode             *self,
                     const char               *key,
                     GtkRestorable            *source,
                     GtkStateNodeBindingFlags  flags)
{
  GtkExpression *expr;

  g_return_if_fail (GTK_IS_STATE_NODE (self));
  g_return_if_fail (key);
  g_return_if_fail (GTK_IS_RESTORABLE (source));

  expr = gtk_constant_expression_new (GTK_TYPE_RESTORABLE, source);
  gtk_state_node_bind_expression (self, key, expr, flags);
  gtk_expression_unref (expr);
}

/**
 * gtk_state_node_bind_property:
 * @self: a `GtkStateNode`
 * @key: the name of the binding
 * @object: the object whose property will be bound
 * @name: the name of the property to bind
 * @flags: flags for this binding
 *
 * Convenience method for binding a property to @self. Constructs a
 * `GtkPropertyExpression` and calls [method@Gtk.StateNode.bind_expression].
 *
 * Note that @name must be a writable property on @object.
 */
void
gtk_state_node_bind_property (GtkStateNode             *self,
                              const char               *key,
                              GObject                  *object,
                              const char               *name,
                              GtkStateNodeBindingFlags  flags)
{
  GtkExpression *obj_expr, *prop_expr;

  g_return_if_fail (GTK_IS_STATE_NODE (self));
  g_return_if_fail (key);
  g_return_if_fail (G_IS_OBJECT (object));
  g_return_if_fail (name);

  obj_expr = gtk_constant_expression_new (G_TYPE_OBJECT, object);
  prop_expr = gtk_property_expression_new (G_TYPE_OBJECT,
                                           g_steal_pointer (&obj_expr),
                                           name);
  gtk_state_node_bind_expression (self, key, prop_expr, flags);
  gtk_expression_unref (prop_expr);
}

/**
 * gtk_state_node_bind_constant:
 * @self: a `GtkStateNode`
 * @key: the name of the binding
 * @flags: flags for this binding
 * @type: the type of the constant value
 * @...: arguments to create the value from
 *
 * Convenience method for binding a constant value to @self. The binding
 * will be save-only. Constructs a `GtkConstantExpression` and calls
 * [method@Gtk.StateNode.bind_expression].
 */
void
gtk_state_node_bind_constant (GtkStateNode             *self,
                              const char               *key,
                              GtkStateNodeBindingFlags  flags,
                              GType                     type,
                              ...)
{
  GValue value = G_VALUE_INIT;
  GtkExpression *expression;
  va_list ap;
  char *error;

  g_return_if_fail (GTK_IS_STATE_NODE (self));
  g_return_if_fail (key);

  flags |= GTK_STATE_NODE_BINDING_FLAGS_SAVE_ONLY;

  va_start (ap, type);
  G_VALUE_COLLECT_INIT (&value, type,
                        ap, G_VALUE_NOCOPY_CONTENTS,
                        &error);
  if (error)
    {
      g_critical ("%s: %s", G_STRLOC, error);
      g_free (error);
      /* We intentionally leak the value here, it might not be in a valid
       * state if an error occurred */
      return;
    }
  expression = gtk_constant_expression_new_for_value (&value);
  g_value_unset (&value);
  va_end (ap);

  gtk_state_node_bind_expression (self, key, expression, flags);
  gtk_expression_unref (expression);
}

/**
 * gtk_state_node_unbind:
 * @self: a `GtkStateNode`
 * @key: the name of the binding
 *
 * Removes the binding named @key from @self.
 *
 * Returns: %TRUE if the binding was found and removed
 */
gboolean
gtk_state_node_unbind (GtkStateNode *self,
                       const char   *key)
{
  g_return_val_if_fail (GTK_IS_STATE_NODE (self), FALSE);
  g_return_val_if_fail (key, FALSE);

  return g_hash_table_remove (self->bindings, key);
}
