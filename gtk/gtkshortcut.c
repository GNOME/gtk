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

#include "config.h"

#include "gtkshortcut.h"

#include "gtkintl.h"
#include "gtkshortcuttrigger.h"
#include "gtkwidget.h"

/**
 * SECTION:gtkshortcut
 * @title: GtkShortcut
 * @short_description: A widget for displaying shortcut
 * @see_also: #GtkShortcutController, #GtkShortcutTrigger
 *
 * GtkShortcut is the low level object used for managing keyboard
 * shortcuts.
 *
 * It contains a description of how to trigger the shortcut via a
 * #GtkShortcutTrigger and a way to activate the shortcut on a widget
 * with gtk_shortcut_activate().
 *
 * The actual work is usually done via #GtkShortcutController, which
 * decides if and when to activate a shortcut. Using that controller
 * directly however is rarely necessary as Various higher level
 * convenience APIs exist on #GtkWidgets that make it easier to use
 * shortcuts in GTK.
 *
 * #GtkShortcut does provide functionality to make it easy for users
 * to work with shortcuts, either by providing informational strings
 * for display purposes or by allowing shortcuts to be configured.
 */

struct _GtkShortcut
{
  GObject parent_instance;

  GtkShortcutTrigger *trigger;
  char *signal;
  GtkShortcutFunc callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
  GVariant *args;
};

enum
{
  PROP_0,
  PROP_ARGUMENTS,
  PROP_CALLBACK,
  PROP_SIGNAL,
  PROP_TRIGGER,

  N_PROPS
};

G_DEFINE_TYPE (GtkShortcut, gtk_shortcut, G_TYPE_OBJECT)

static GParamSpec *properties[N_PROPS] = { NULL, };

static void
gtk_shortcut_dispose (GObject *object)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  g_clear_pointer (&self->trigger, gtk_shortcut_trigger_unref);
  g_clear_pointer (&self->signal, g_free);
  g_clear_pointer (&self->args, g_variant_unref);
  if (self->callback)
    {
      if (self->destroy_notify)
        self->destroy_notify (self->user_data);

      self->callback = NULL;
      self->user_data = NULL;
      self->destroy_notify = NULL;
    }

  G_OBJECT_CLASS (gtk_shortcut_parent_class)->dispose (object);
}

static void
gtk_shortcut_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  switch (property_id)
    {
    case PROP_ARGUMENTS:
      g_value_set_variant (value, self->args);
      break;

    case PROP_CALLBACK:
      g_value_set_boolean (value, self->callback != NULL);
      break;

    case PROP_SIGNAL:
      g_value_set_string (value, self->signal);
      break;

    case PROP_TRIGGER:
      g_value_set_boxed (value, self->trigger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_shortcut_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GtkShortcut *self = GTK_SHORTCUT (object);

  switch (property_id)
    {
    case PROP_ARGUMENTS:
      gtk_shortcut_set_arguments (self, g_value_get_variant (value));
      break;

    case PROP_SIGNAL:
      gtk_shortcut_set_signal (self, g_value_get_string (value));
      break;

    case PROP_TRIGGER:
      gtk_shortcut_set_trigger (self, g_value_dup_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_shortcut_class_init (GtkShortcutClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_shortcut_dispose;
  gobject_class->get_property = gtk_shortcut_get_property;
  gobject_class->set_property = gtk_shortcut_set_property;

  /**
   * GtkShortcut:arguments:
   *
   * Arguments passed to activation.
   */
  properties[PROP_ARGUMENTS] =
    g_param_spec_variant ("arguments",
                          P_("Arguments"),
                          P_("Arguments passed to activation"),
                          G_VARIANT_TYPE_ANY,
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcut:callback:
   *
   * Whether a callback is used for shortcut activation
   */
  properties[PROP_CALLBACK] =
    g_param_spec_boolean ("callback",
                          P_("Callback"),
                          P_("Whether a callback is used for shortcut activation"),
                          FALSE,
                          G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcut:signal:
   *
   * The action signal to emit on the widget upon activation.
   */
  properties[PROP_SIGNAL] =
    g_param_spec_string ("signal",
                         P_("Signal"),
                         P_("The action signal to emit"),
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GtkShortcut:trigger:
   *
   * The trigger that triggers this shortcut.
   */
  properties[PROP_TRIGGER] =
    g_param_spec_boxed ("trigger",
                        P_("Trigger"),
                        P_("The trigger for this shortcut"),
                        GTK_TYPE_SHORTCUT_TRIGGER,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gtk_shortcut_init (GtkShortcut *self)
{
  self->trigger = gtk_shortcut_trigger_ref (gtk_never_trigger_get ());
}

/**
 * gtk_shortcut_new:
 *
 * Creates a new empty #GtkShortcut that never triggers and activates nothing.
 *
 * Returns: a new #GtkShortcut
 **/
GtkShortcut *
gtk_shortcut_new (void)
{
  return g_object_new (GTK_TYPE_SHORTCUT, NULL);
}

static gboolean
binding_compose_params (GObject         *object,
                        GVariantIter    *args,
                        GSignalQuery    *query,
                        GValue         **params_p)
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
  g_value_set_object (params, G_OBJECT (object));
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
gtk_shortcut_emit_signal (GObject    *object,
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

  signal_id = g_signal_lookup (signal, G_OBJECT_TYPE (object));
  if (!signal_id)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "Could not find signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (object)));
      return FALSE;
    }

  g_signal_query (signal_id, &query);
  if (args)
    n_args = g_variant_iter_init (&args_iter, args);
  else
    n_args = 0;
  if (query.n_params != n_args ||
      (query.return_type != G_TYPE_NONE && query.return_type != G_TYPE_BOOLEAN) ||
      !binding_compose_params (object, &args_iter, &query, &params))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signature mismatch for signal \"%s\" in the '%s' class ancestry",
                   signal,
                   g_type_name (G_OBJECT_TYPE (object)));
      return FALSE;
    }
  else if (!(query.signal_flags & G_SIGNAL_ACTION))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   "signal \"%s\" in the '%s' class ancestry cannot be used for action emissions",
                   signal,
                   g_type_name (G_OBJECT_TYPE (object)));
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

gboolean
gtk_shortcut_activate (GtkShortcut *self,
                       GtkWidget   *widget)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), FALSE);
  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);

  if (self->callback)
    {
      return self->callback (widget, self->args, self->user_data);
    }
  else if (self->signal)
    {
      GError *error = NULL;
      gboolean handled;

      if (!gtk_shortcut_emit_signal (G_OBJECT (widget),
                                     self->signal,
                                     self->args,
                                     &handled,
                                     &error))
        {
          char *accelerator = gtk_shortcut_trigger_to_string (self->trigger);
          g_warning ("gtk_shortcut_activate(): \":%s\": %s",
                     accelerator,
                     error->message);
          g_clear_error (&error);
          return FALSE;
        }

      return handled;
    }
  else
    {
      /* shortcut is a dud */
      return FALSE;
    }
}

/**
 * gtk_shortcut_get_trigger:
 * @self: a #GtkShortcut
 *
 * Gets the trigger used to trigger @self.
 *
 * Returns: (transfer none): the trigger used
 **/
GtkShortcutTrigger *
gtk_shortcut_get_trigger (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->trigger;
}

/**
 * gtk_shortcut_set_trigger:
 * @self: a #GtkShortcut
 * @trigger: (transfer full) (nullable): The new trigger.
 *     If the @trigger is %NULL, the never trigger will be used.
 *
 * Sets the new trigger for @self to be @trigger.
 **/
void
gtk_shortcut_set_trigger (GtkShortcut *self,
                          GtkShortcutTrigger *trigger)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (trigger == NULL)
    trigger = gtk_shortcut_trigger_ref (gtk_never_trigger_get ());

  if (self->trigger == trigger)
    {
      gtk_shortcut_trigger_unref (trigger);
      return;
    }
  
  gtk_shortcut_trigger_unref (self->trigger);
  self->trigger = trigger;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRIGGER]);
}

GVariant *
gtk_shortcut_get_arguments (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->args;
}

void
gtk_shortcut_set_arguments (GtkShortcut *self,
                            GVariant    *args)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (self->args == args)
    return;
  
  g_clear_pointer (&self->args, g_variant_unref);
  if (args)
    self->args = g_variant_ref_sink (args);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ARGUMENTS]);
}

static void
gtk_shortcut_clear_activation (GtkShortcut *self)
{
  if (self->signal)
    {
      g_clear_pointer (&self->signal, g_free);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGNAL]);
    }

  if (self->callback)
    {
      if (self->destroy_notify)
        self->destroy_notify (self->user_data);

      self->callback = NULL;
      self->user_data = NULL;
      self->destroy_notify = NULL;

      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CALLBACK]);
    }
}

const char *
gtk_shortcut_get_signal (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), NULL);

  return self->signal;
}

void
gtk_shortcut_set_signal (GtkShortcut *self,
                         const gchar *signal)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  if (g_strcmp0 (self->signal, signal) == 0)
    return;
  
  g_object_freeze_notify (G_OBJECT (self));

  gtk_shortcut_clear_activation (self);
  self->signal = g_strdup (signal);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SIGNAL]);

  g_object_thaw_notify (G_OBJECT (self));
}

gboolean
gtk_shortcut_has_callback (GtkShortcut *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT (self), FALSE);

  return self->callback != NULL;
}

void
gtk_shortcut_set_callback (GtkShortcut     *self,
                           GtkShortcutFunc  callback,
                           gpointer         data,
                           GDestroyNotify   destroy)
{
  g_return_if_fail (GTK_IS_SHORTCUT (self));

  g_object_freeze_notify (G_OBJECT (self));

  gtk_shortcut_clear_activation (self);

  self->callback = callback;
  self->user_data = data;
  self->destroy_notify = destroy;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CALLBACK]);

  g_object_thaw_notify (G_OBJECT (self));
}

