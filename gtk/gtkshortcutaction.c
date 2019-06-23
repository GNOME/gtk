/*
 * Copyright © 2018 Benjamin Otte
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

/**
 * SECTION:gtkshortcutaction
 * @Title: GtkShortcutAction
 * @Short_description: Actions to track if shortcuts should be activated
 * @See_also: #GtkShortcut
 *
 * #GtkShortcutAction is the object used to track if a #GtkShortcut should be
 * activated. For this purpose, gtk_shortcut_action_action() can be called
 * on a #GdkEvent.
 *
 * #GtkShortcutActions contain functions that allow easy presentation to end
 * users as well as being printed for debugging.
 *
 * All #GtkShortcutActions are immutable, you can only specify their properties
 * during construction. If you want to change a action, you have to replace it
 * with a new one.
 */

#include "config.h"

#include "gtkshortcutactionprivate.h"

#include "gtkbuilder.h"
#include "gtkwidgetprivate.h"

typedef struct _GtkShortcutActionClass GtkShortcutActionClass;

#define GTK_IS_SHORTCUT_ACTION_TYPE(action,type) (GTK_IS_SHORTCUT_ACTION (action) && (action)->action_class->action_type == (type))

struct _GtkShortcutAction
{
  const GtkShortcutActionClass *action_class;

  volatile int ref_count;
};

struct _GtkShortcutActionClass
{
  GtkShortcutActionType action_type;
  gsize struct_size;
  const char *type_name;

  void            (* finalize)    (GtkShortcutAction            *action);
  gboolean        (* activate)    (GtkShortcutAction            *action,
                                   GtkShortcutActionFlags        flags,
                                   GtkWidget                    *widget,
                                   GVariant                     *args);
  void            (* print)       (GtkShortcutAction            *action,
                                   GString                      *string);
};

G_DEFINE_BOXED_TYPE (GtkShortcutAction, gtk_shortcut_action,
                     gtk_shortcut_action_ref,
                     gtk_shortcut_action_unref)

static void
gtk_shortcut_action_finalize (GtkShortcutAction *self)
{
  self->action_class->finalize (self);

  g_free (self);
}

/*< private >
 * gtk_shortcut_action_new:
 * @action_class: class structure for this action
 *
 * Returns: (transfer full): the newly created #GtkShortcutAction
 */
static GtkShortcutAction *
gtk_shortcut_action_new (const GtkShortcutActionClass *action_class)
{
  GtkShortcutAction *self;

  g_return_val_if_fail (action_class != NULL, NULL);

  self = g_malloc0 (action_class->struct_size);

  self->action_class = action_class;

  self->ref_count = 1;

  return self;
}

/**
 * gtk_shortcut_action_ref:
 * @self: a #GtkShortcutAction
 *
 * Acquires a reference on the given #GtkShortcutAction.
 *
 * Returns: (transfer full): the #GtkShortcutAction with an additional reference
 */
GtkShortcutAction *
gtk_shortcut_action_ref (GtkShortcutAction *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

/**
 * gtk_shortcut_action_unref:
 * @self: (transfer full): a #GtkShortcutAction
 *
 * Releases a reference on the given #GtkShortcutAction.
 *
 * If the reference was the last, the resources associated to the @self
 * are freed.
 */
void
gtk_shortcut_action_unref (GtkShortcutAction *self)
{
  g_return_if_fail (GTK_IS_SHORTCUT_ACTION (self));

  if (g_atomic_int_dec_and_test (&self->ref_count))
    gtk_shortcut_action_finalize (self);
}

/**
 * gtk_shortcut_action_get_action_type:
 * @self: a #GtkShortcutAction
 *
 * Returns the type of the @action.
 *
 * Returns: the type of the #GtkShortcutAction
 */
GtkShortcutActionType
gtk_shortcut_action_get_action_type (GtkShortcutAction *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), GTK_SHORTCUT_ACTION_NOTHING);

  return self->action_class->action_type;
}

/**
 * gtk_shortcut_action_to_string:
 * @self: a #GtkShortcutAction
 *
 * Prints the given action into a human-readable string.
 * This is a small wrapper around gtk_shortcut_action_print() to help
 * when debugging.
 *
 * Returns: (transfer full): a new string
 **/
char *
gtk_shortcut_action_to_string (GtkShortcutAction *self)
{
  GString *string;

  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), NULL);

  string = g_string_new (NULL);
  gtk_shortcut_action_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_shortcut_action_print:
 * @self: a #GtkShortcutAction
 * @string: a #GString to print into
 *
 * Prints the given action into a string for the developer.
 * This is meant for debugging and logging.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 **/
void
gtk_shortcut_action_print (GtkShortcutAction *self,
                           GString           *string)
{
  g_return_if_fail (GTK_IS_SHORTCUT_ACTION (self));
  g_return_if_fail (string != NULL);

  return self->action_class->print (self, string);
}

/**
 * gtk_shortcut_action_activate:
 * @self: a #GtkShortcutAction
 * @flags: flags to activate with
 * @widget: Target of the activation
 * @args: (allow-none): arguments to pass
 *
 * Activates the action on the @widget with the given @args. 
 *
 * Note that some actions do ignore the passed in @flags, @widget or
 * @args.
 *
 * Activation of an action can fail for various reasons. If the action
 * is not supported by the @widget, if the @args don't match the action
 * or if the activation otherwise had no effect, %FALSE will be returned.
 *
 * Returns: %TRUE if this action was activated successfully
 **/
gboolean
gtk_shortcut_action_activate (GtkShortcutAction      *self,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION (self), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  return self->action_class->activate (self, flags, widget, args);
}

static char *
string_is_function (const char *string,
                    const char *function_name)
{
  gsize len;

  if (!g_str_has_prefix (string, function_name))
    return NULL;
  string += strlen (function_name);

  if (string[0] != '(')
    return NULL;
  string ++;

  len = strlen (string);
  if (len == 0 || string[len - 1] != ')')
    return NULL;

  return g_strndup (string, len - 1);
}

GtkShortcutAction *
gtk_shortcut_action_parse_builder (GtkBuilder  *builder,
                                   const char  *string,
                                   GError     **error)
{
  GtkShortcutAction *result;
  char *arg;

  if (g_str_equal (string, "nothing"))
    return gtk_nothing_action_new ();
  if (g_str_equal (string, "activate"))
    return gtk_activate_action_new ();
  if (g_str_equal (string, "mnemonic-activate"))
    return gtk_mnemonic_action_new ();

  if ((arg = string_is_function (string, "action")))
    {
      result = gtk_action_action_new (arg);
      g_free (arg);
    }
  else if ((arg = string_is_function (string, "signal")))
    {
      result = gtk_signal_action_new (arg);
      g_free (arg);
    }
  else if ((arg = string_is_function (string, "callback")))
    {
      GtkShortcutFunc callback;

      callback = (GtkShortcutFunc) gtk_builder_lookup_callback_symbol (builder, arg);
      if (!callback)
        {
          static GModule *module = NULL;

          if (G_UNLIKELY (module == NULL))
            module = g_module_open (NULL, 0);

          if (!g_module_symbol (module, arg, (gpointer) &callback))
            {
              g_set_error (error,
                           GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                           "No function named \"%s\" found", arg);
              g_free (arg);
              return NULL;
            }
        }
      
      result = gtk_callback_action_new (callback, NULL, NULL);
      g_free (arg);
    }
  else if ((arg = string_is_function (string, "gaction")))
    {
      GObject *object = gtk_builder_get_object (builder, arg);

      if (object == NULL)
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                       "Action with ID \"%s\" not found", arg);
          g_free (arg);
          return NULL;
        }
      else if (!G_IS_ACTION (object))
        {
          g_set_error (error,
                       GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_ID,
                       "Object with ID \"%s\" is not a GAction", arg);
          g_free (arg);
          return NULL;
        }

      result = gtk_gaction_action_new (G_ACTION (object));
      g_free (arg);
    }
  else
    {
      g_set_error (error,
                   GTK_BUILDER_ERROR, GTK_BUILDER_ERROR_INVALID_VALUE,
                   "String \"%s\" does not specify a GtkShortcutAction", string);
      return NULL;
    }

  return result;
}

/*** GTK_SHORTCUT_ACTION_NOTHING ***/

typedef struct _GtkNothingAction GtkNothingAction;

struct _GtkNothingAction
{
  GtkShortcutAction action;
};

static void
gtk_nothing_action_finalize (GtkShortcutAction *action)
{
  g_assert_not_reached ();
}

static gboolean
gtk_nothing_action_activate (GtkShortcutAction      *action,
                             GtkShortcutActionFlags  flags,
                             GtkWidget              *widget,
                             GVariant               *args)
{
  return FALSE;
}

static void
gtk_nothing_action_print (GtkShortcutAction *action,
                          GString           *string)
{
  g_string_append (string, "nothing");
}

static const GtkShortcutActionClass GTK_NOTHING_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_NOTHING,
  sizeof (GtkNothingAction),
  "GtkNothingAction",
  gtk_nothing_action_finalize,
  gtk_nothing_action_activate,
  gtk_nothing_action_print
};

static GtkNothingAction nothing = { { &GTK_NOTHING_ACTION_CLASS, 1 } };

/**
 * gtk_nothing_action_new:
 *
 * Gets the nothing action. This is an action that does nothing and where
 * activating it always fails.
 *
 * Returns: The nothing action
 */
GtkShortcutAction *
gtk_nothing_action_new (void)
{
  return gtk_shortcut_action_ref (&nothing.action);
}

/*** GTK_SHORTCUT_ACTION_CALLBACK ***/

typedef struct _GtkCallbackAction GtkCallbackAction;

struct _GtkCallbackAction
{
  GtkShortcutAction action;

  GtkShortcutFunc callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
};

static void
gtk_callback_action_finalize (GtkShortcutAction *action)
{
  GtkCallbackAction *self = (GtkCallbackAction *) action;

  if (self->destroy_notify)
    self->destroy_notify (self->user_data);
}

static gboolean
gtk_callback_action_activate (GtkShortcutAction      *action,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  GtkCallbackAction *self = (GtkCallbackAction *) action;

  return self->callback (widget, args, self->user_data);
}

static void
gtk_callback_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  GtkCallbackAction *self = (GtkCallbackAction *) action;

  g_string_append_printf (string, "callback(%p)", self->callback);
}

static const GtkShortcutActionClass GTK_CALLBACK_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_CALLBACK,
  sizeof (GtkCallbackAction),
  "GtkCallbackAction",
  gtk_callback_action_finalize,
  gtk_callback_action_activate,
  gtk_callback_action_print
};

/**
 * gtk_callback_action_new:
 * @callback: the callback to call
 * @data: 
 * @destroy: 
 *
 * Create a custom action that calls the given @callback when
 * activated.
 *
 * Returns: A new shortcut action
 **/
GtkShortcutAction *
gtk_callback_action_new (GtkShortcutFunc         callback,
                         gpointer                data,
                         GDestroyNotify          destroy)
{
  GtkCallbackAction *self;

  g_return_val_if_fail (callback != NULL, NULL);

  self = (GtkCallbackAction *) gtk_shortcut_action_new (&GTK_CALLBACK_ACTION_CLASS);

  self->callback = callback;
  self->user_data = data;
  self->destroy_notify = destroy;

  return &self->action;
}

/*** GTK_SHORTCUT_ACTION_ACTIVATE ***/

typedef struct _GtkActivateAction GtkActivateAction;

struct _GtkActivateAction
{
  GtkShortcutAction action;
};

static void
gtk_activate_action_finalize (GtkShortcutAction *action)
{
  g_assert_not_reached ();
}

static gboolean
gtk_activate_action_activate (GtkShortcutAction      *action,
                             GtkShortcutActionFlags  flags,
                             GtkWidget              *widget,
                             GVariant               *args)
{
  return gtk_widget_activate (widget);
}

static void
gtk_activate_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  g_string_append (string, "activate");
}

static const GtkShortcutActionClass GTK_ACTIVATE_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_ACTIVATE,
  sizeof (GtkActivateAction),
  "GtkActivateAction",
  gtk_activate_action_finalize,
  gtk_activate_action_activate,
  gtk_activate_action_print
};

static GtkActivateAction activate = { { &GTK_ACTIVATE_ACTION_CLASS, 1 } };

/**
 * gtk_activate_action_new:
 *
 * Gets the activate action. This is an action that calls gtk_widget_activate()
 * on the given widget upon activation.
 *
 * Returns: The activate action
 */
GtkShortcutAction *
gtk_activate_action_new (void)
{
  return gtk_shortcut_action_ref (&activate.action);
}

/*** GTK_SHORTCUT_ACTION_MNEMONIC ***/

typedef struct _GtkMnemonicAction GtkMnemonicAction;

struct _GtkMnemonicAction
{
  GtkShortcutAction action;
};

static void
gtk_mnemonic_action_finalize (GtkShortcutAction *action)
{
  g_assert_not_reached ();
}

static gboolean
gtk_mnemonic_action_activate (GtkShortcutAction      *action,
                              GtkShortcutActionFlags  flags,
                              GtkWidget              *widget,
                              GVariant               *args)
{
  return gtk_widget_mnemonic_activate (widget, flags & GTK_SHORTCUT_ACTION_EXCLUSIVE ? FALSE : TRUE);
}

static void
gtk_mnemonic_action_print (GtkShortcutAction *action,
                           GString           *string)
{
  g_string_append (string, "mnemonic-activate");
}

static const GtkShortcutActionClass GTK_MNEMONIC_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_MNEMONIC,
  sizeof (GtkMnemonicAction),
  "GtkMnemonicAction",
  gtk_mnemonic_action_finalize,
  gtk_mnemonic_action_activate,
  gtk_mnemonic_action_print
};

static GtkMnemonicAction mnemonic = { { &GTK_MNEMONIC_ACTION_CLASS, 1 } };

/**
 * gtk_mnemonic_action_new:
 *
 * Gets the mnemonic action. This is an action that calls
 * gtk_widget_mnemonic_activate() on the given widget upon activation.
 *
 * Returns: The mnemonic action
 */
GtkShortcutAction *
gtk_mnemonic_action_new (void)
{
  return gtk_shortcut_action_ref (&mnemonic.action);
}

/*** GTK_SHORTCUT_ACTION_SIGNAL ***/

typedef struct _GtkSignalAction GtkSignalAction;

struct _GtkSignalAction
{
  GtkShortcutAction action;

  char *name;
};

static void
gtk_signal_action_finalize (GtkShortcutAction *action)
{
  GtkSignalAction *self = (GtkSignalAction *) action;

  g_free (self->name);
}

static gboolean
binding_compose_params (GtkWidget     *widget,
                        GVariantIter  *args,
                        GSignalQuery  *query,
                        GValue       **params_p)
{
  GValue *params;
  const GType *types;
  guint i;
  gboolean valid;

  params = g_new0 (GValue, query->n_params + 1);
  *params_p = params;

  /* The instance we emit on is the first object in the array
   */
  g_value_init (params, G_TYPE_OBJECT);
  g_value_set_object (params, G_OBJECT (widget));
  params++;

  types = query->param_types;
  valid = TRUE;
  for (i = 1; i < query->n_params + 1 && valid; i++)
    {
      GValue tmp_value = G_VALUE_INIT;
      GVariant *tmp_variant;

      g_value_init (params, *types);
      tmp_variant = g_variant_iter_next_value (args);

      switch ((guint) g_variant_classify (tmp_variant))
        {
        case G_VARIANT_CLASS_BOOLEAN:
          g_value_init (&tmp_value, G_TYPE_BOOLEAN);
          g_value_set_boolean (&tmp_value, g_variant_get_boolean (tmp_variant));
          break;
        case G_VARIANT_CLASS_DOUBLE:
          g_value_init (&tmp_value, G_TYPE_DOUBLE);
          g_value_set_double (&tmp_value, g_variant_get_double (tmp_variant));
          break;
        case G_VARIANT_CLASS_INT32:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_int32 (tmp_variant));
          break;
        case G_VARIANT_CLASS_UINT32:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_uint32 (tmp_variant));
          break;
        case G_VARIANT_CLASS_INT64:
          g_value_init (&tmp_value, G_TYPE_LONG);
          g_value_set_long (&tmp_value, g_variant_get_int64 (tmp_variant));
          break;
        case G_VARIANT_CLASS_STRING:
          /* gtk_rc_parse_flags/enum() has fancier parsing for this; we can't call
           * that since we don't have a GParamSpec, so just do something simple
           */
          if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_ENUM)
            {
              GEnumClass *class = G_ENUM_CLASS (g_type_class_ref (*types));
              GEnumValue *enum_value;
              const char *s = g_variant_get_string (tmp_variant, NULL);

              valid = FALSE;

              enum_value = g_enum_get_value_by_name (class, s);
              if (!enum_value)
                enum_value = g_enum_get_value_by_nick (class, s);

              if (enum_value)
                {
                  g_value_init (&tmp_value, *types);
                  g_value_set_enum (&tmp_value, enum_value->value);
                  valid = TRUE;
                }

              g_type_class_unref (class);
            }
          /* This is just a hack for compatibility with GTK+-1.2 where a string
           * could be used for a single flag value / without the support for multiple
           * values in gtk_rc_parse_flags(), this isn't very useful.
           */
          else if (G_TYPE_FUNDAMENTAL (*types) == G_TYPE_FLAGS)
            {
              GFlagsClass *class = G_FLAGS_CLASS (g_type_class_ref (*types));
              GFlagsValue *flags_value;
              const char *s = g_variant_get_string (tmp_variant, NULL);

              valid = FALSE;

              flags_value = g_flags_get_value_by_name (class, s);
              if (!flags_value)
                flags_value = g_flags_get_value_by_nick (class, s);
              if (flags_value)
                {
                  g_value_init (&tmp_value, *types);
                  g_value_set_flags (&tmp_value, flags_value->value);
                  valid = TRUE;
                }

              g_type_class_unref (class);
            }
          else
            {
              g_value_init (&tmp_value, G_TYPE_STRING);
              g_value_set_static_string (&tmp_value, g_variant_get_string (tmp_variant, NULL));
            }
          break;
        default:
          valid = FALSE;
          break;
        }

      if (valid)
        {
          if (!g_value_transform (&tmp_value, params))
            valid = FALSE;

          g_value_unset (&tmp_value);
        }

      g_variant_unref (tmp_variant);
      types++;
      params++;
    }

  if (!valid)
    {
      guint j;

      for (j = 0; j < i; j++)
        g_value_unset (&(*params_p)[j]);

      g_free (*params_p);
      *params_p = NULL;
    }

  return valid;
}

static gboolean
gtk_signal_action_emit_signal (GtkWidget *widget,
                               const char *signal,
                               GVariant   *args,
                               gboolean   *handled,
                               GError    **error)
{
  GSignalQuery query;
  guint signal_id;
  GValue *params = NULL;
  GValue return_val = G_VALUE_INIT;
  GVariantIter args_iter;
  gsize n_args;
  guint i;

  *handled = FALSE;

  signal_id = g_signal_lookup (signal, G_OBJECT_TYPE (widget));
  if (!signal_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not find signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }

  g_signal_query (signal_id, &query);
  if (args == NULL)
    n_args = 0;
  else if (g_variant_is_of_type (args, G_VARIANT_TYPE_TUPLE))
    n_args = g_variant_iter_init (&args_iter, args);
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "argument GVariant is not a tuple");
      return FALSE;
    }
  if (query.n_params != n_args ||
      (query.return_type != G_TYPE_NONE && query.return_type != G_TYPE_BOOLEAN) ||
      !binding_compose_params (widget, &args_iter, &query, &params))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signature mismatch for signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }
  else if (!(query.signal_flags & G_SIGNAL_ACTION))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signal \"%s\" in the '%s' class ancestry cannot be used for action emissions",
                   signal,
                   g_type_name (G_OBJECT_TYPE (widget)));
      return FALSE;
    }

  if (query.return_type == G_TYPE_BOOLEAN)
    g_value_init (&return_val, G_TYPE_BOOLEAN);

  g_signal_emitv (params, signal_id, 0, &return_val);

  if (query.return_type == G_TYPE_BOOLEAN)
    {
      if (g_value_get_boolean (&return_val))
        *handled = TRUE;
      g_value_unset (&return_val);
    }
  else
    *handled = TRUE;

  if (params != NULL)
    {
      for (i = 0; i < query.n_params + 1; i++)
        g_value_unset (&params[i]);

      g_free (params);
    }

  return TRUE;
}

static gboolean
gtk_signal_action_activate (GtkShortcutAction      *action,
                            GtkShortcutActionFlags  flags,
                            GtkWidget              *widget,
                            GVariant               *args)
{
  GtkSignalAction *self = (GtkSignalAction *) action;
  GError *error = NULL;
  gboolean handled;

  if (!gtk_signal_action_emit_signal (widget,
                                      self->name,
                                      args,
                                      &handled,
                                      &error))
    {
      g_warning ("gtk_signal_action_activate(): %s",
                 error->message);
      g_clear_error (&error);
      return FALSE;
    }

  return handled;
}

static void
gtk_signal_action_print (GtkShortcutAction *action,
                         GString           *string)
{
  GtkSignalAction *self = (GtkSignalAction *) action;

  g_string_append_printf (string, "signal(%s)", self->name);
}

static const GtkShortcutActionClass GTK_SIGNAL_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_SIGNAL,
  sizeof (GtkSignalAction),
  "GtkSignalAction",
  gtk_signal_action_finalize,
  gtk_signal_action_activate,
  gtk_signal_action_print
};

/**
 * gtk_signal_action_new:
 * @signal_name: name of the signal to emit
 *
 * Creates an action that when activated, emits the given action signal
 * on the provided widget unpacking the given args into arguments passed
 * to the signal.
 *
 * Returns: a new #GtkShortcutAction
 **/
GtkShortcutAction *
gtk_signal_action_new (const char *signal_name)
{
  GtkSignalAction *self;

  g_return_val_if_fail (signal_name != NULL, NULL);

  self = (GtkSignalAction *) gtk_shortcut_action_new (&GTK_SIGNAL_ACTION_CLASS);

  self->name = g_strdup (signal_name);

  return &self->action;
}

/**
 * gtk_signal_action_get_signal_name:
 * @action: a signal action
 *
 * Returns the name of the signal that will be emitted.
 *
 * Returns: the name of the signal to emit
 **/
const char *
gtk_signal_action_get_signal_name (GtkShortcutAction *action)
{
  GtkSignalAction *self = (GtkSignalAction *) action;

  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION_TYPE (action, GTK_SHORTCUT_ACTION_SIGNAL), NULL);

  return self->name;
}

/*** GTK_SHORTCUT_ACTION_ACTION ***/

typedef struct _GtkActionAction GtkActionAction;

struct _GtkActionAction
{
  GtkShortcutAction action;

  char *name;
};

static void
gtk_action_action_finalize (GtkShortcutAction *action)
{
  GtkSignalAction *self = (GtkSignalAction *) action;

  g_free (self->name);
}

static gboolean
gtk_shortcut_trigger_check_parameter_type (GVariant           *args,
                                           const GVariantType *parameter_type)
{
  if (args)
    {
      if (parameter_type == NULL)
        {
          g_warning ("Trying to invoke action with arguments, but action has no parameter");
          return FALSE;
        }

      if (!g_variant_is_of_type (args, parameter_type))
        {
          gchar *typestr = g_variant_type_dup_string (parameter_type);
          gchar *targetstr = g_variant_print (args, TRUE);
          g_warning ("Trying to invoke action with target '%s',"
                     " but action expects parameter with type '%s'", targetstr, typestr);
          g_free (targetstr);
          g_free (typestr);
          return FALSE;
        }
    }
  else
    {
      if (parameter_type != NULL)
        {
          gchar *typestr = g_variant_type_dup_string (parameter_type);
          g_warning ("Trying to invoke action without arguments,"
                     " but action expects parameter with type '%s'", typestr);
          g_free (typestr);
          return FALSE;
        }
    }

  return TRUE;
}

static gboolean
gtk_action_action_activate (GtkShortcutAction      *action,
                            GtkShortcutActionFlags  flags,
                            GtkWidget              *widget,
                            GVariant               *args)
{
  GtkActionAction *self = (GtkActionAction *) action;
  GActionGroup *action_group;
  const GVariantType *parameter_type;
  gboolean enabled;

  action_group = G_ACTION_GROUP (_gtk_widget_get_action_muxer (widget, FALSE));
  if (action_group == NULL)
    return FALSE;

  if (!g_action_group_query_action (action_group, self->name, &enabled, &parameter_type, NULL, NULL, NULL))
    return FALSE;

  if (!enabled)
    return FALSE;

  /* We found an action with the correct name and it's enabled.
   * This is the action that we are going to try to invoke.
   *
   * There is still the possibility that the args don't
   * match the expected parameter type.  In that case, we will print
   * a warning.
   */
  if (!gtk_shortcut_trigger_check_parameter_type (args, parameter_type))
    return FALSE;

  g_action_group_activate_action (action_group, self->name, args);

  return TRUE;
}

static void
gtk_action_action_print (GtkShortcutAction *action,
                         GString           *string)
{
  GtkActionAction *self = (GtkActionAction *) action;

  g_string_append_printf (string, "action(%s)", self->name);
}

static const GtkShortcutActionClass GTK_ACTION_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_ACTION,
  sizeof (GtkActionAction),
  "GtkActionAction",
  gtk_action_action_finalize,
  gtk_action_action_activate,
  gtk_action_action_print
};

/**
 * gtk_action_action_new:
 * @name: the detailed name of the action
 *
 * Creates an action that when activated, activates the action given by
 * the detailed @name on the widget passing the given arguments to it.
 *
 * See gtk_widget_insert_action_group() for how to add actions to widgets.
 *
 * Returns: a new #GtkShortcutAction
 **/
GtkShortcutAction *
gtk_action_action_new (const char *name)
{
  GtkActionAction *self;

  g_return_val_if_fail (name != NULL, NULL);

  self = (GtkActionAction *) gtk_shortcut_action_new (&GTK_ACTION_ACTION_CLASS);

  self->name = g_strdup (name);

  return &self->action;
}

/**
 * gtk_action_action_get_name:
 * @action: an action action
 *
 * Returns the name of the action that will be activated.
 *
 * Returns: the name of the action to activate
 **/
const char *
gtk_action_action_get_name (GtkShortcutAction *action)
{
  GtkActionAction *self = (GtkActionAction *) action;

  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION_TYPE (action, GTK_SHORTCUT_ACTION_ACTION), NULL);

  return self->name;
}

/*** GTK_SHORTCUT_ACTION_GACTION ***/

typedef struct _GtkGActionAction GtkGActionAction;

struct _GtkGActionAction
{
  GtkShortcutAction action;

  GAction *gaction;
};

static void
gtk_gaction_action_finalize (GtkShortcutAction *action)
{
  GtkGActionAction *self = (GtkGActionAction *) action;

  g_object_unref (self->gaction);
}

static gboolean
gtk_gaction_action_activate (GtkShortcutAction      *action,
                             GtkShortcutActionFlags  flags,
                             GtkWidget              *widget,
                             GVariant               *args)
{
  GtkGActionAction *self = (GtkGActionAction *) action;

  if (!gtk_shortcut_trigger_check_parameter_type (args, g_action_get_parameter_type (self->gaction)))
    return FALSE;

  if (!g_action_get_enabled (self->gaction))
    return FALSE;

  g_action_activate (self->gaction, args);

  return TRUE;
}

static void
gtk_gaction_action_print (GtkShortcutAction *action,
                          GString           *string)
{
  GtkGActionAction *self = (GtkGActionAction *) action;

  g_string_append_printf (string, "gaction(%s %p)", g_action_get_name (self->gaction), self->gaction);
}

static const GtkShortcutActionClass GTK_GACTION_ACTION_CLASS = {
  GTK_SHORTCUT_ACTION_GACTION,
  sizeof (GtkGActionAction),
  "GtkGActionAction",
  gtk_gaction_action_finalize,
  gtk_gaction_action_activate,
  gtk_gaction_action_print
};

/**
 * gtk_gaction_action_new:
 * @action: a #GAction
 *
 * Creates a new action that will activate the given @gaction when activated
 * with the passed in arguments.
 *
 * Returns: a new #GtkShortcutAction
 **/
GtkShortcutAction *
gtk_gaction_action_new (GAction *action)
{
  GtkGActionAction *self;

  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  self = (GtkGActionAction *) gtk_shortcut_action_new (&GTK_GACTION_ACTION_CLASS);

  self->gaction = g_object_ref (action);

  return &self->action;
}

/**
 * gtk_gaction_action_get_gaction:
 * @action: a gaction action
 *
 * Queries the #GAction that will be activated when this action is activated.
 *
 * Returns: (transfer none): The #GAction that will be activated
 **/
GAction *
gtk_gaction_action_get_gaction (GtkShortcutAction *action)
{
  GtkGActionAction *self = (GtkGActionAction *) action;

  g_return_val_if_fail (GTK_IS_SHORTCUT_ACTION_TYPE (action, GTK_SHORTCUT_ACTION_GACTION), NULL);

  return self->gaction;
}

