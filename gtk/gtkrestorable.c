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

#include "gtkrestorableprivate.h"

/**
 * GtkSaveContext:
 *
 * Object that's used to construct the transient state tree snapshot for
 * [iface@Gtk.Restorable]. The app fills state into this context, which is then
 * serialized to disk to be later restored.
 *
 * Each `GtkSaveContext` represents a node in the app's state tree snapshot, as
 * that snapshot is being constructed. Conceptually, you can think of each node
 * in the state tree as `GVariant` dictionary (`a{sv}`). Each node can thus store
 * some serialized state from the app. The children of each node are themselves
 * just `GVariant` dictionaries that are inserted into the parent's dictionary
 * under some name.
 *
 * The app's state tree is not a suitable place to store bulk amounts of data.
 * Instead, GTK manages a special bulk state directory. The app can create files
 * inside this directory, and then store the absolute path to those files in the
 * state tree. You can obtain the bulk state directory by calling
 * [method@Gtk.SaveContext.get_bulk_state_dir].
 *
 * Since: 4.24
 */

struct _GtkSaveContext {
  gatomicrefcount ref_count;

  GHashTable *state;
  GHashTable *children;

  GFile *bulk_state_dir;
};

G_DEFINE_BOXED_TYPE (GtkSaveContext, gtk_save_context,
                     gtk_save_context_ref, gtk_save_context_unref)

GtkSaveContext *
gtk_save_context_ref (GtkSaveContext *self)
{
  g_return_val_if_fail (self, NULL);
  g_atomic_ref_count_inc (&self->ref_count);
  return self;
}

void
gtk_save_context_unref (GtkSaveContext *self)
{
  g_return_if_fail (self);

  if (!g_ref_count_dec (&self->ref_count))
    return;

  g_clear_pointer (&self->state, g_hash_table_unref);
  g_clear_pointer (&self->children, g_hash_table_unref);
  g_clear_object (&self->bulk_state_dir);
  g_free (self);
}

static GtkSaveContext *
gtk_save_context_new_common (void)
{
  GtkSaveContext *self = g_new0 (GtkSaveContext, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->state = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify) g_variant_unref);
  self->children = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                          (GDestroyNotify) gtk_save_context_unref);
  return self;
}

/*<private>
 * gtk_save_context_new_root:
 * @bulk_dir: The bulk directory to use for this state tree
 *
 * Constructs an empty `GtkSaveContext` at the root of a new state tree
 *
 * Returns: a new `GtkSaveContext`
 */
GtkSaveContext *
gtk_save_context_new_root (GFile *bulk_dir)
{
  GtkSaveContext *self;

  g_return_val_if_fail (G_IS_FILE (bulk_dir), NULL);

  self = gtk_save_context_new_common ();
  self->bulk_state_dir = g_object_ref (bulk_dir);
  return self;
}

/**
 * gtk_save_context_child:
 * @self: a `GtkSaveContext`
 * @key: the name to use for the new sub-tree
 *
 * Creates a new `GtkSaveContext` for collecting the state of a sub-tree, inserts
 * it into @self under the provided @key, and returns the new context.
 *
 * Returns: the new `GtkSaveContext` for the sub-tree
 */
GtkSaveContext *
gtk_save_context_child (GtkSaveContext *self,
                        const char     *key)
{
  GtkSaveContext *child;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (key, NULL);

  child = gtk_save_context_new_common ();

  g_hash_table_insert (self->children,
                       g_strdup (key),
                       gtk_save_context_ref (child));

  child->bulk_state_dir = g_object_ref (self->bulk_state_dir);

  return child;
}

/**
 * gtk_save_context_get_bulk_state_dir:
 * @self: a `GtkSaveContext`
 *
 * Returns a path to a directory that you can use to save bulky state. It is
 * your responsibility to make sure that you pick unique file names inside of
 * this directory.
 *
 * Returns: (transfer none): The bulk state directory
 */
GFile *
gtk_save_context_get_bulk_state_dir (GtkSaveContext *self)
{
  return self->bulk_state_dir;
}

/**
 * gtk_save_context_insert:
 * @self: a `GtkSaveContext`
 * @key: the name to use for the entry in @self
 * @format_string: a [struct@GLib.Variant] format string
 * @...: arguments appropriate for @format_string
 *
 * This is a convenience function that calls [ctor@GLib.Variant.new] for
 * @format_string and uses the result to call [method@Gtk.SaveContext.insert_value].
 */
void
gtk_save_context_insert (GtkSaveContext *self,
                         const char     *key,
                         const char     *format_string,
                         ...)
{
  va_list ap;
  GVariant *value;

  g_return_if_fail (self);
  g_return_if_fail (key);
  g_return_if_fail (format_string);

  va_start (ap, format_string);
  value = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  gtk_save_context_insert_value (self, key, value);
}

/**
 * gtk_save_context_insert_value:
 * @self: a `GtkSaveContext`
 * @key: the name to use for the entry in @self
 * @value: the value to insert into @self
 *
 * Inserts @value into @self under the specified @key.
 */
void
gtk_save_context_insert_value (GtkSaveContext *self,
                               const char     *key,
                               GVariant       *value)
{
  g_return_if_fail (self);
  g_return_if_fail (key);
  g_return_if_fail (value);

  g_hash_table_insert (self->state,
                       g_strdup (key),
                       g_variant_ref_sink (value));
}

static void
fill_builder_recursive (GtkSaveContext *self,
                        GVariantBuilder *builder)
{
  GHashTableIter iter;
  gpointer key, value;

  g_hash_table_iter_init (&iter, self->state);
  while (g_hash_table_iter_next (&iter, &key, &value))
    g_variant_builder_add (builder, "{sv}", key, value);

  g_hash_table_iter_init (&iter, self->children);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      g_variant_builder_open (builder, G_VARIANT_TYPE ("{sv}"));
      g_variant_builder_add (builder, "s", key);
      g_variant_builder_open (builder, G_VARIANT_TYPE ("v"));
      g_variant_builder_open (builder, G_VARIANT_TYPE ("a{sv}"));

      fill_builder_recursive (value, builder);

      g_variant_builder_close (builder);
      g_variant_builder_close (builder);
      g_variant_builder_close (builder);
    }
}

/*<private>
 * gtk_save_context_serialize:
 * @self: a `GtkSaveContext`
 *
 * Serializes @self into a new `GVariant`
 *
 * Returns: a new `GVariant`
 */
GVariant *
gtk_save_context_serialize (GtkSaveContext *self)
{
  GVariantBuilder builder;
  GVariant *variant;

  g_return_val_if_fail (self, NULL);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));
  fill_builder_recursive (self, &builder);
  variant = g_variant_builder_end (&builder);
  g_variant_ref_sink (variant);
  return variant;
}

/**
 * GtkRestoreContext:
 *
 * Object that represents a loaded state tree snapshot to restore for
 * [iface@Gtk.Restorable]. GTK loads serialized state from disk and deserializes
 * it into this object for restoration.
 *
 * Each `GtkRestoreContext` represents a node in the app's state tree snapshot,
 * as it was stored into a [struct@Gtk.SaveContext] by a previous instance of
 * the app.
 *
 * Keep in mind that the state being restored might have been saved by a different
 * version of the app. Since one of the major goals of the session save/restore
 * infrastructure is to make updates less burdensome for users, your app should
 * make an effort where possible to restore state saved by previous versions of
 * itself. However, your app should be ready for downgrades as well, where it is
 * permissible to simply discard the saved state.
 *
 * Each `GtkRestoreContext` comes with an [enum@Gtk.RestoreReason], which tells
 * the app how much state it should restore. GTK, technically, restores the app's
 * state even when it launches normally and shouldn't be doing a complete state
 * restoration. This is because apps may want to restore some of their state
 * even during a normal launch. This commonly includes the app window's last used
 * size and position, but could also include more state like the last-viewed page
 * in the app's primary navigation.
 *
 * Since: 4.24
 */

struct _GtkRestoreContext {
  gatomicrefcount ref_count;

  GVariantDict *state;
  GtkRestoreReason reason;
};

G_DEFINE_BOXED_TYPE (GtkRestoreContext, gtk_restore_context,
                     gtk_restore_context_ref, gtk_restore_context_unref)

GtkRestoreContext *
gtk_restore_context_ref (GtkRestoreContext *self)
{
  g_return_val_if_fail (self, NULL);
  g_atomic_ref_count_inc (&self->ref_count);
  return self;
}

void
gtk_restore_context_unref (GtkRestoreContext *self)
{
  g_return_if_fail (self);

  if (!g_ref_count_dec (&self->ref_count))
    return;

  g_clear_pointer (&self->state, g_variant_dict_unref);
  g_free (self);
}

/*<private>
 * gtk_restore_context_new:
 * @state: The state loaded from disk
 * @reason: The restore reason
 *
 * Constructs a new `GtkRestoreContext` from the state loaded from disk.
 *
 * Returns: a new `GtkRestoreContext`
 */
GtkRestoreContext *
gtk_restore_context_new (GVariant         *state,
                         GtkRestoreReason  reason)
{
  GtkRestoreContext *self;

  g_return_val_if_fail (g_variant_is_of_type (state, G_VARIANT_TYPE_VARDICT),
                        NULL);

  self = g_new (GtkRestoreContext, 1);
  g_atomic_ref_count_init (&self->ref_count);
  self->state = g_variant_dict_new (state);
  self->reason = reason;
  return self;
}

/**
 * gtk_restore_context_child:
 * @self: a `GtkRestoreContext`
 * @key: the name of the sub-tree to look up
 *
 * Looks up a sub-tree named @key in @self, and constructs a new
 * `GtkRestoreContext` for that sub-tree.
 *
 * Returns: (nullable): the new `GtkRestoreContext`, or %NULL if not found
 */
GtkRestoreContext *
gtk_restore_context_child (GtkRestoreContext *self,
                           const char        *key)
{
  GVariant *state;

  g_return_val_if_fail (self, NULL);
  g_return_val_if_fail (key, NULL);

  state = g_variant_dict_lookup_value (self->state, key,
                                       G_VARIANT_TYPE_VARDICT);
  if (!state)
    return NULL;

  return gtk_restore_context_new (state, self->reason);
}

/**
 * gtk_restore_context_get_state:
 * @self: a `GtkRestoreContext`
 *
 * Gets the underlying [struct@GLib.VariantDict] that contains the state to
 * restore.
 *
 * Returns: (transfer none): a `GVariantDict`
 */
GVariantDict *
gtk_restore_context_get_state (GtkRestoreContext *self)
{
  return self->state;
}

/**
 * gtk_restore_context_get_reason:
 * @self: a `GtkRestoreContext`
 *
 * Gets the reason that the app is restoring state. You can use this to determine
 * how much of the state should be restored.
 *
 * Returns: the restore reason
 */
GtkRestoreReason
gtk_restore_context_get_reason (GtkRestoreContext *self)
{
  return self->reason;
}

/**
 * gtk_restore_context_contains:
 * @self: a `GtkRestoreContext`
 * @key: the name of the entry to look up in @self
 *
 * Convenience method for calling [method@GLib.VariantDict.contains] on
 * the underlying `GVariantDict`. See [method@Gtk.RestoreContext.get_state].
 *
 * Returns: %TRUE if @self contains the @key, %FALSE otherwise
 */
gboolean
gtk_restore_context_contains (GtkRestoreContext *self,
                              const char        *key)
{
  return g_variant_dict_contains (self->state, key);
}


/**
 * gtk_restore_context_lookup:
 * @self: a `GtkRestoreContext`
 * @key: the name of the entry to look up in @self
 * @format_string: a [struct@GLib.Variant] format string
 * @...: Arguments to unpack the value into
 *
 * Convenience method for calling [method@GLib.VariantDict.lookup] on
 * the underlying `GVariantDict`. See [method@Gtk.RestoreContext.get_state].
 *
 * Returns: %TRUE if a value was unpacked
 */
gboolean
gtk_restore_context_lookup (GtkRestoreContext *self,
                            const char        *key,
                            const char        *format_string,
                            ...)
{
  GVariant *value;
  va_list ap;

  /* Adapted from GLib's g_variant_dict_lookup:
   * https://gitlab.gnome.org/GNOME/glib/-/blob/6522dca1a609cfa2084b979018635e49211919df/glib/gvariant.c#L4086-4132
   */

  g_return_val_if_fail (self, FALSE);
  g_return_val_if_fail (key, FALSE);
  g_return_val_if_fail (format_string, FALSE);

  value = g_variant_dict_lookup_value (self->state, key, NULL);
  if (!value || !g_variant_check_format_string (value, format_string, FALSE))
    return FALSE;

  va_start (ap, format_string);
  g_variant_get_va (value, format_string, NULL, &ap);
  va_end (ap);

  return TRUE;
}

/**
 * gtk_restore_context_lookup_value:
 * @self: a `GtkRestoreContext`
 * @key: the name of the entry to look up in @self
 * @expected_type: the expected type of the `GVariant`
 *
 * Convenience method for calling [method@GLib.VariantDict.lookup_value] on
 * the underlying `GVariantDict`. See [method@Gtk.RestoreContext.get_state].
 *
 * Returns: (nullable): a `GVariant`, or %NULL if none is found
 */
GVariant *
gtk_restore_context_lookup_value (GtkRestoreContext  *self,
                                  const char         *key,
                                  const GVariantType *expected_type)
{
  return g_variant_dict_lookup_value (self->state, key, expected_type);
}

/**
 * GtkRestorable:
 *
 * Core interface for session save/restore infrastructure.
 *
 * ## Motivation
 *
 * GTK's session save and restore infrastructure allows apps to capture their
 * current transient state and later load it back. On supported platforms, all
 * of the user's running apps will be saved at log-out time and relaunched on
 * the next log-in. This infrastructure allows the app to restore the transient
 * state, and the user can quickly get back to what they were doing before logging
 * out.
 *
 * One of the main goals here is to reduce the burden of updates for users, both
 * for the app itself and the platform underneath. Additionally, this same
 * infrastructure makes apps more resilient against crashes of both the app
 * and the OS: after the crash, the app can be re-launched and restored to
 * a snapshot taken shortly before the crash.
 *
 * ## Basic Implementation
 *
 * `GtkRestorable` objects form themselves into a tree, but that tree is opaque
 * to GTK. GTK interacts with the root `GtkRestorable` (likely your `GtkWindow`),
 * and that object passes requests down to its children if it has any.
 *
 * This interface is designed to allow for both synchronous and asynchronous
 * implementations. Objects can implement whichever vfuncs are most convenient
 * for their situation.
 *
 * When saving the app's state, GTK will construct a [struct@Gtk.SaveContext]
 * and then ask the `GtkRestorable` tree to capture the current state into the
 * context (via [vfunc@Gtk.Restorable.save_state] or
 * [vfunc@Gtk.Restorable.save_state_async]/[vfunc@Gtk.Restorable.save_state_finish]).
 * The result is a snapshot of the app's current transient state, which gets
 * written to disk.
 *
 * Later, GTK may attempt to restore the app's state. GTK will load the state
 * from disk into a [struct@Gtk.RestoreContext], and then give it to the
 * `GtkRestorable` tree (via [vfunc@Gtk.Restorable.restore_state] or
 * [vfunc@Gtk.Restorable.restore_state_async]/[vfunc@Gtk.Restorable.restore_state_finish]).
 * See the documentation for `GtkRestoreContext` for more guidance about
 * restoring state.
 *
 * ## Dirty tracking
 *
 * This interface includes a state invalidation feature that allows GTK to avoid
 * state collection when it is unnecessary. This is particularly useful in
 * conjunction with GTK's periodic auto-save feature.
 *
 * To opt in, an implementation can override [vfunc@Gtk.Restorable.check_state_dirty].
 * Return `TRUE` if the state has changed since the last state collection, and
 * GTK will re-collect state during the app's next save. Note that GTK may still
 * collect the object's state even if you return `FALSE`.
 *
 * Since: 4.24
 */


G_DEFINE_INTERFACE (GtkRestorable, gtk_restorable, G_TYPE_OBJECT)

static void
gtk_restorable_default_init (GtkRestorableInterface *iface)
{
}

static void
restorable_save_state_cb (GObject      *source_object,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  GtkRestorable *self = GTK_RESTORABLE (source_object);
  GtkRestorableInterface *iface = GTK_RESTORABLE_GET_IFACE (self);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (iface->save_state_finish (self, result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_clear_object (&task);
}

/**
 * gtk_restorable_save_state:
 * @self: a `GtkRestorable`
 * @context: the `GtkSaveContext` to fill with state
 * @cancellable: (nullable): A `GCancellable`
 * @callback: (nullable): A `GAsyncReadyCallback` to call once we're done filling the @context
 * @user_data: (nullable): The data to pass to the callback function
 *
 * Asynchronously take a snapshot of the transient state in @self and collect it
 * into the @context
 */
void
gtk_restorable_save_state (GtkRestorable       *self,
                           GtkSaveContext      *context,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GTask *task;
  GtkRestorableInterface *iface;

  g_return_if_fail (GTK_IS_RESTORABLE (self));
  iface = GTK_RESTORABLE_GET_IFACE (self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_source_tag (task, gtk_restorable_save_state);

  if (iface->save_state)
    iface->save_state (self, context);

  if (iface->save_state_async)
    {
      g_return_if_fail (iface->save_state_finish);
      iface->save_state_async (self,
                               context,
                               g_task_get_cancellable (task),
                               restorable_save_state_cb,
                               g_object_ref (task));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_clear_object (&task);
}

/**
 * gtk_restorable_save_state_finish:
 * @self: a `GtkRestorable`
 * @result: a `GAsyncResult`
 * @error: a `GError` location to store the error should one occur
 *
 * Finishes a call to [method@Gtk.Restorable.save_state].
 *
 * Returns: %TRUE if successful
 */
gboolean
gtk_restorable_save_state_finish (GtkRestorable  *self,
                                  GAsyncResult   *result,
                                  GError        **error)
{
  g_return_val_if_fail (GTK_IS_RESTORABLE (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
restorable_restore_state_cb (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  GtkRestorable *self = GTK_RESTORABLE (source_object);
  GtkRestorableInterface *iface = GTK_RESTORABLE_GET_IFACE (self);
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (iface->restore_state_finish (self, result, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, error);

  g_clear_object (&task);
}

/**
 * gtk_restorable_restore_state:
 * @self: a `GtkRestorable`
 * @context: the `GtkRestoreContext` to read from
 * @cancellable: (nullable): A `GCancellable`
 * @callback: (nullable): A `GAsyncReadyCallback` to call once we're done restoring from @context
 * @user_data: (nullable): The data to pass to the callback function
 *
 * Asynchronously restore @self from the state snapshot in @context
 */
void
gtk_restorable_restore_state (GtkRestorable       *self,
                              GtkRestoreContext   *context,
                              GCancellable        *cancellable,
                              GAsyncReadyCallback  callback,
                              gpointer             user_data)
{
  GTask *task;
  GtkRestorableInterface *iface;

  g_return_if_fail (GTK_IS_RESTORABLE (self));
  iface = GTK_RESTORABLE_GET_IFACE (self);

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_return_on_cancel (task, TRUE);
  g_task_set_source_tag (task, gtk_restorable_restore_state);

  if (iface->restore_state)
    iface->restore_state (self, context);

  if (iface->restore_state_async)
    {
      g_return_if_fail (iface->restore_state_finish);
      iface->restore_state_async (self,
                                  context,
                                  g_task_get_cancellable (task),
                                  restorable_restore_state_cb,
                                  g_object_ref (task));
    }
  else
    {
      g_task_return_boolean (task, TRUE);
    }

  g_clear_object (&task);
}

/**
 * gtk_restorable_restore_state_finish:
 * @self: a `GtkRestorable`
 * @result: a `GAsyncResult`
 * @error: a `GError` location to store the error should one occur
 *
 * Finishes a call to [method@Gtk.Restorable.restore_state].
 *
 * Returns: %TRUE if successful
 */
gboolean
gtk_restorable_restore_state_finish (GtkRestorable *self,
                                     GAsyncResult  *result,
                                     GError **error)
{
  g_return_val_if_fail (GTK_IS_RESTORABLE (self), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, self), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

/*<private>
 * gtk_restorable_check_state_dirty:
 * @self: a `GtkRestorable`
 *
 * Checks if the state in @self has changed since the last time it was collected,
 * and thus GTK needs to re-collect the state.
 *
 * Returns: whether @self is marked dirty
 */
gboolean
gtk_restorable_check_state_dirty (GtkRestorable *self)
{
  GtkRestorableInterface *iface;

  g_return_val_if_fail (GTK_IS_RESTORABLE (self), FALSE);
  iface = GTK_RESTORABLE_GET_IFACE (self);

  if (iface->check_state_dirty)
    return iface->check_state_dirty (self);

  /* If not implemented, we assume that the state is always dirty */
  return TRUE;
}
