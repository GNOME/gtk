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

#include "gtksaveprivate.h"

#include <gio/gio.h>

/**
 * GtkSave:
 *
 * Handle to an in-progress save operation of application state, as part of GTK's
 * session save/restore integration. This object is created by GTK and given to
 * the application via the [signal@GtkApplication::save-state] and
 * [signal@Gtk.ApplicationWindow::save-state] signals.
 *
 * Applications use this object to communicate the state they wish to save to
 * GTK. They can do so asynchronously by deferring the completion of the
 * operation via [method@Gtk.Save.defer].
 *
 * An example usage might look like this:
 *
 * ```
 * static void
 * finished_cb (GObject      *object,
 *              GAsyncResult *result,
 *              gpointer      data)
 * {
 *   g_autoptr (GtkSave) state = GTK_SAVE (data);
 *   g_autofree char *output = NULL;
 *
 *   output = my_encuablator_encabulate_finish(MY_ENCABULATOR (object), result);
 *
 *   gtk_save_insert (state, "encabulator-output", "s", output);
 *   gtk_save_complete (state);
 * }
 *
 * static void
 * on_app_save_state (GtkApplication *gapp,
 *                    GtkSave        *state)
 * {
 *   MyApp *app = MY_APP (gapp);
 *
 *   gtk_save_insert (state, "something", "b", app->something);
 *   gtk_save_insert (state, "something-else", "u", app->something_else);
 *
 *   gtk_save_defer (state);
 *   my_encabulator_encabulate (app->encabulator, finished_cb, g_object_ref (state));
 * }
 * ```
 *
 * Since: 4.22
 */

struct _GtkSave
{
  GObject parent_instance;

  GVariantDict *dict;
  guint defer_count;

  GtkSaveCallback callback;
  gpointer user_data;
};

G_DEFINE_FINAL_TYPE (GtkSave, gtk_save, G_TYPE_OBJECT)

static void
gtk_save_init (GtkSave *save)
{
  save->dict = g_variant_dict_new (NULL);
}

static void
gtk_save_finalize (GObject *object)
{
  GtkSave *save = GTK_SAVE (object);

  g_clear_pointer (&save->dict, g_variant_dict_unref);

  G_OBJECT_CLASS (gtk_save_parent_class)->finalize (object);
}

static void
gtk_save_class_init (GtkSaveClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_save_finalize;
}

static gboolean
gtk_save_is_valid (GtkSave *save)
{
  return GTK_IS_SAVE (save) && save->dict != NULL;
}

/**
 * gtk_save_insert:
 * @save: a [class@Gtk.Save]
 * @key: the key to insert the value for
 * @format_string: a [class@GLib.Variant] format string
 * @...: arguments, as per @format_string
 *
 * Inserts a value into the [class@Gtk.Save].
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling [ctor@GLib.Variant.new] followed by [method@Gtk.Save.insert_value].
 *
 * Since: 4.22
 */
void
gtk_save_insert (GtkSave    *save,
                 const char *key,
                 const char *format_string,
                 ...)
{
  va_list ap;
  GVariant *value;

  g_return_if_fail (gtk_save_is_valid (save));
  g_return_if_fail (key != NULL);
  g_return_if_fail (format_string != NULL);

  va_start (ap, format_string);
  value = g_variant_new_va (format_string, NULL, &ap);
  va_end (ap);

  g_variant_dict_insert_value (save->dict, key, value);
}

/**
 * gtk_save_insert_value:
 * @save: a [class@Gtk.Save]
 * @key: the key to insert the value for
 * @value: the value to insert
 *
 * Inserts a @value into the [class@Gtk.Save]. The @value is consumed
 * if it is floating.
 *
 * This call is a convenience wrapper that is exactly equivalent to
 * calling [method@GLib.VariantDict.insert_value] on the underlying
 * [class@GLib.VariantDict].
 *
 * Since: 4.22
 */
void
gtk_save_insert_value (GtkSave    *save,
                       const char *key,
                       GVariant   *value)
{
  g_return_if_fail (gtk_save_is_valid (save));
  g_return_if_fail (key != NULL);
  g_return_if_fail (value != NULL);

  g_variant_dict_insert_value (save->dict, key, value);
}

/**
 * gtk_save_get_dict:
 * @save: a [class@Gtk.Save]
 *
 * Gets the underlying [class@GLib.VariantDict].
 *
 * Returns: (transfer full): the underlying [class@GLib.VariantDict]
 *
 * Since: 4.22
 */
GVariantDict *
gtk_save_get_dict (GtkSave *save)
{
  g_return_val_if_fail (gtk_save_is_valid (save), NULL);
  return g_variant_dict_ref (save->dict);
}

/**
 * gtk_save_defer:
 * @save: a [class@Gtk.Save]
 *
 * Increases the defer count of the handle. This indicates that an asynchronous
 * operation is still going on, and that the application is not yet done
 * populating this object.
 *
 * Once the asynchronous operation is done, the application should call
 * [method@Gtk.Save.complete]. Each call to [method@Gtk.Save.defer] must
 * correspond to a call to [method@Gtk.Save.complete].
 *
 * Since: 4.22
 */
void
gtk_save_defer (GtkSave *save)
{
  g_return_if_fail (gtk_save_is_valid (save));

  save->defer_count++;
}

/**
 * gtk_save_complete:
 * @save: a [class@Gtk.Save]
 *
 * Decreases the defer count of the handle. This indicates that an asynchronous
 * operation was completed.
 *
 * Each call to [method@Gtk.Save.complete] must correspond to a call to
 * [method@Gtk.Save.defer].
 *
 * Once the count reaches zero, the application is done populating this object
 * and the save operation will complete. After this, it is not permissible to
 * use this handle except for reference counting operations.
 *
 * Since: 4.22
 */
void
gtk_save_complete (GtkSave *save)
{
  GVariant *result;

  g_return_if_fail (gtk_save_is_valid (save));
  g_return_if_fail (save->defer_count > 0);

  save->defer_count--;

  if (save->defer_count == 0)
    {
      result = g_variant_ref_sink (g_variant_dict_end (save->dict));
      save->callback(result, save->user_data);
      g_variant_unref (result);

      /* Invalidate the object */
      g_clear_pointer (&save->dict, g_variant_dict_unref);
    }
}

GtkSave *
gtk_save_new (GtkSaveCallback callback,
              gpointer        data)
{
  GtkSave *save = g_object_new (GTK_TYPE_SAVE, NULL);
  save->callback = callback;
  save->user_data = data;
  return save;
}
