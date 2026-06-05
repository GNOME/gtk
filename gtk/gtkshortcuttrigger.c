/*
 * Copyright © 2018 Benjamin Otte
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * GtkShortcutTrigger:
 *
 * Tracks how a `GtkShortcut` can be activated.
 *
 * To find out if a `GtkShortcutTrigger` triggers, you can call
 * [method@Gtk.ShortcutTrigger.trigger] on a `GdkEvent`.
 *
 * `GtkShortcutTriggers` contain functions that allow easy presentation
 * to end users as well as being printed for debugging.
 *
 * All `GtkShortcutTriggers` are immutable, you can only specify their
 * properties during construction. If you want to change a trigger, you
 * have to replace it with a new one.
 */

#include "config.h"

#include "gtkshortcuttrigger.h"

#include "gtkaccelgroupprivate.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"

#define GTK_SHORTCUT_TRIGGER_HASH_NEVER         0u
#define GTK_SHORTCUT_TRIGGER_HASH_KEYVAL        1u
#define GTK_SHORTCUT_TRIGGER_HASH_MNEMONIC      2u
#define GTK_SHORTCUT_TRIGGER_HASH_ALTERNATIVE   3u


/* {{{ GObject boilerplate */

struct _GtkShortcutTrigger
{
  GObject parent_instance;
};

struct _GtkShortcutTriggerClass
{
  GObjectClass parent_class;

  GdkKeyMatch     (* trigger)     (GtkShortcutTrigger  *trigger,
                                   GdkEvent            *event,
                                   gboolean             enable_mnemonics);
  guint           (* hash)        (GtkShortcutTrigger  *trigger);
  int             (* compare)     (GtkShortcutTrigger  *trigger1,
                                   GtkShortcutTrigger  *trigger2);
  void            (* print)       (GtkShortcutTrigger  *trigger,
                                   GString             *string);
  gboolean        (* print_label) (GtkShortcutTrigger  *trigger,
                                   GdkDisplay          *display,
                                   GString             *string);
};

G_DEFINE_ABSTRACT_TYPE (GtkShortcutTrigger, gtk_shortcut_trigger, G_TYPE_OBJECT)

static void
gtk_shortcut_trigger_class_init (GtkShortcutTriggerClass *klass)
{
}

static void
gtk_shortcut_trigger_init (GtkShortcutTrigger *self)
{
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_shortcut_trigger_trigger:
 * @self: a `GtkShortcutTrigger`
 * @event: the event to check
 * @enable_mnemonics: %TRUE if mnemonics should trigger. Usually the
 *   value of this property is determined by checking that the passed
 *   in @event is a Key event and has the right modifiers set.
 *
 * Checks if the given @event triggers @self.
 *
 * Returns: Whether the event triggered the shortcut
 */
GdkKeyMatch
gtk_shortcut_trigger_trigger (GtkShortcutTrigger *self,
                              GdkEvent           *event,
                              gboolean            enable_mnemonics)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), GDK_KEY_MATCH_NONE);

  return GTK_SHORTCUT_TRIGGER_GET_CLASS (self)->trigger (self, event, enable_mnemonics);
}

/**
 * gtk_shortcut_trigger_parse_string: (constructor)
 * @string: the string to parse
 *
 * Tries to parse the given string into a trigger.
 *
 * On success, the parsed trigger is returned.
 * When parsing failed, %NULL is returned.
 *
 * The accepted strings are:
 *
 *   - `never`, for `GtkNeverTrigger`
 *   - a string parsed by gtk_accelerator_parse(), for a `GtkKeyvalTrigger`, e.g. `<Control>C`
 *   - underscore, followed by a single character, for `GtkMnemonicTrigger`, e.g. `_l`
 *   - two or more valid trigger strings, separated by a `|` character, for a
 *     `GtkAlternativeTrigger`: `<Control>q|<Control>w`
 *
 * Note that you will have to escape the `<` and `>` characters when specifying
 * triggers in XML files, such as GtkBuilder ui files. Use `&lt;` instead of
 * `<` and `&gt;` instead of `>`.
 *
 * Returns: (nullable) (transfer full): a new `GtkShortcutTrigger`
 */
GtkShortcutTrigger *
gtk_shortcut_trigger_parse_string (const char *string)
{
  GdkModifierType modifiers;
  guint keyval;
  const char *sep;

  g_return_val_if_fail (string != NULL, NULL);

  if ((sep = strchr (string, '|')) != NULL)
    {
      const char *rest = string;
      GtkShortcutTrigger *triggers[20];
      unsigned int n_triggers = 0;

      while ((sep = strchr (rest, '|')) != NULL)
        {
          char *frag;

          if (n_triggers + 1 == G_N_ELEMENTS (triggers))
            {
              g_warning ("Can't handle more than 20 triggers in GtkAlternativeTrigger");
              return gtk_alternative_trigger_newv (triggers, n_triggers);
            }

          frag = g_strndup (rest, sep - rest);
          rest = sep + 1;

          if (frag[0] == '\0')
            {
              g_free (frag);
              for (unsigned int i = 0; i < n_triggers; i++)
                g_object_unref (triggers[i]);
              return NULL;
            }

          triggers[n_triggers] = gtk_shortcut_trigger_parse_string (frag);
          if (triggers[n_triggers] == NULL)
            {
              for (unsigned int i = 0; i < n_triggers; i++)
                g_object_unref (triggers[i]);
              return NULL;
            }

          n_triggers++;
          g_free (frag);
        }

      if (rest[0] == '\0')
        {
          for (unsigned int i = 0; i < n_triggers; i++)
            g_object_unref (triggers[i]);
          return NULL;
        }
      g_assert (n_triggers < 20);
      triggers[n_triggers] = gtk_shortcut_trigger_parse_string (rest);
      n_triggers++;

      return gtk_alternative_trigger_newv (triggers, n_triggers);
    }

  if (g_str_equal (string, "never"))
    return g_object_ref (gtk_never_trigger_get ());

  if (string[0] == '_')
    {
      keyval = gdk_keyval_from_name (string + 1);
      if (keyval != GDK_KEY_VoidSymbol)
        return gtk_mnemonic_trigger_new (gdk_keyval_to_lower (keyval));
    }

  if (gtk_accelerator_parse (string, &keyval, &modifiers))
    return gtk_keyval_trigger_new (keyval, modifiers);

  return NULL;
}

/**
 * gtk_shortcut_trigger_to_string:
 * @self: a `GtkShortcutTrigger`
 *
 * Prints the given trigger into a human-readable string.
 *
 * This is a small wrapper around [method@Gtk.ShortcutTrigger.print]
 * to help when debugging.
 *
 * Returns: (transfer full): a new string
 */
char *
gtk_shortcut_trigger_to_string (GtkShortcutTrigger *self)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new (NULL);

  gtk_shortcut_trigger_print (self, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_shortcut_trigger_print:
 * @self: a `GtkShortcutTrigger`
 * @string: a `GString` to print into
 *
 * Prints the given trigger into a string for the developer.
 * This is meant for debugging and logging.
 *
 * The form of the representation may change at any time
 * and is not guaranteed to stay identical.
 */
void
gtk_shortcut_trigger_print (GtkShortcutTrigger *self,
                            GString            *string)
{
  g_return_if_fail (GTK_IS_SHORTCUT_TRIGGER (self));
  g_return_if_fail (string != NULL);

  GTK_SHORTCUT_TRIGGER_GET_CLASS (self)->print (self, string);
}

/**
 * gtk_shortcut_trigger_to_label:
 * @self: a `GtkShortcutTrigger`
 * @display: `GdkDisplay` to print for
 *
 * Gets textual representation for the given trigger.
 *
 * This function is returning a translated string for
 * presentation to end users for example in menu items
 * or in help texts.
 *
 * The @display in use may influence the resulting string in
 * various forms, such as resolving hardware keycodes or by
 * causing display-specific modifier names.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 *
 * Returns: (transfer full): a new string
 */
char *
gtk_shortcut_trigger_to_label (GtkShortcutTrigger *self,
                               GdkDisplay         *display)
{
  GString *string;

  g_return_val_if_fail (self != NULL, NULL);

  string = g_string_new (NULL);
  gtk_shortcut_trigger_print_label (self, display, string);

  return g_string_free (string, FALSE);
}

/**
 * gtk_shortcut_trigger_print_label:
 * @self: a `GtkShortcutTrigger`
 * @display: `GdkDisplay` to print for
 * @string: a `GString` to print into
 *
 * Prints the given trigger into a string.
 *
 * This function is returning a translated string for presentation
 * to end users for example in menu items or in help texts.
 *
 * The @display in use may influence the resulting string in
 * various forms, such as resolving hardware keycodes or by
 * causing display-specific modifier names.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 *
 * Returns: %TRUE if something was printed or %FALSE if the
 *   trigger did not have a textual representation suitable
 *   for end users.
 **/
gboolean
gtk_shortcut_trigger_print_label (GtkShortcutTrigger *self,
                                  GdkDisplay         *display,
                                  GString            *string)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  return GTK_SHORTCUT_TRIGGER_GET_CLASS (self)->print_label (self, display, string);
}

/**
 * gtk_shortcut_trigger_hash:
 * @trigger: (type GtkShortcutTrigger): a `GtkShortcutTrigger`
 *
 * Generates a hash value for a `GtkShortcutTrigger`.
 *
 * The output of this function is guaranteed to be the same for a given
 * value only per-process. It may change between different processor
 * architectures or even different versions of GTK. Do not use this
 * function as a basis for building protocols or file formats.
 *
 * The types of @trigger is `gconstpointer` only to allow use of this
 * function with `GHashTable`. They must each be a `GtkShortcutTrigger`.
 *
 * Returns: a hash value corresponding to @trigger
 */
guint
gtk_shortcut_trigger_hash (gconstpointer trigger)
{
  GtkShortcutTrigger *t = (GtkShortcutTrigger *) trigger;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (t), 0);

  return GTK_SHORTCUT_TRIGGER_GET_CLASS (t)->hash (t);
}

/**
 * gtk_shortcut_trigger_equal:
 * @trigger1: (type GtkShortcutTrigger): a `GtkShortcutTrigger`
 * @trigger2: (type GtkShortcutTrigger): a `GtkShortcutTrigger`
 *
 * Checks if @trigger1 and @trigger2 trigger under the same conditions.
 *
 * The types of @one and @two are `gconstpointer` only to allow use of this
 * function with `GHashTable`. They must each be a `GtkShortcutTrigger`.
 *
 * Returns: %TRUE if @trigger1 and @trigger2 are equal
 */
gboolean
gtk_shortcut_trigger_equal (gconstpointer trigger1,
                            gconstpointer trigger2)
{
  return gtk_shortcut_trigger_compare (trigger1, trigger2) == 0;
}

/**
 * gtk_shortcut_trigger_compare:
 * @trigger1: (type GtkShortcutTrigger): a `GtkShortcutTrigger`
 * @trigger2: (type GtkShortcutTrigger): a `GtkShortcutTrigger`
 *
 * The types of @trigger1 and @trigger2 are `gconstpointer` only to allow
 * use of this function as a `GCompareFunc`.
 *
 * They must each be a `GtkShortcutTrigger`.
 *
 * Returns: An integer less than, equal to, or greater than zero if
 *   @trigger1 is found, respectively, to be less than, to match,
 *   or be greater than @trigger2.
 */
int
gtk_shortcut_trigger_compare (gconstpointer trigger1,
                              gconstpointer trigger2)
{
  GtkShortcutTrigger *t1 = (GtkShortcutTrigger *) trigger1;
  GtkShortcutTrigger *t2 = (GtkShortcutTrigger *) trigger2;
  GType type1, type2;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (t1), -1);
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (t2), 1);

  type1 = G_OBJECT_TYPE (t1);
  type2 = G_OBJECT_TYPE (t2);

  if (type1 == type2)
    {
      return GTK_SHORTCUT_TRIGGER_GET_CLASS (t1)->compare (t1, t2);
    }
  else
    { /* never < keyval < mnemonic < alternative */
      if (type1 == GTK_TYPE_NEVER_TRIGGER ||
          type2 == GTK_TYPE_ALTERNATIVE_TRIGGER)
        return -1;
      if (type2 == GTK_TYPE_NEVER_TRIGGER ||
          type1 == GTK_TYPE_ALTERNATIVE_TRIGGER)
        return 1;

      if (type1 == GTK_TYPE_KEYVAL_TRIGGER)
        return -1;
      else
        return 1;
    }
}

/* }}} */
/* {{{ Never trigger */

struct _GtkNeverTrigger
{
  GtkShortcutTrigger parent_instance;
};

struct _GtkNeverTriggerClass
{
  GtkShortcutTriggerClass parent_class;
};

G_DEFINE_TYPE (GtkNeverTrigger, gtk_never_trigger, GTK_TYPE_SHORTCUT_TRIGGER)

static GtkShortcutTrigger *never_singleton;

static void G_GNUC_NORETURN
gtk_never_trigger_finalize (GObject *gobject)
{
  g_assert_not_reached ();

  G_OBJECT_CLASS (gtk_never_trigger_parent_class)->finalize (gobject);
}

static GObject *
gtk_never_trigger_constructor (GType                  type,
                               guint                  n_construct_params,
                               GObjectConstructParam *construct_params)
{
  if (G_UNLIKELY (never_singleton == NULL))
    {
      GObjectClass *parent_class = G_OBJECT_CLASS (gtk_never_trigger_parent_class);
      never_singleton = GTK_SHORTCUT_TRIGGER (parent_class->constructor (type,
                                                                         n_construct_params,
                                                                         construct_params));
    }
  return g_object_ref (G_OBJECT (never_singleton));
}

static GdkKeyMatch
gtk_never_trigger_trigger (GtkShortcutTrigger *trigger,
                           GdkEvent           *event,
                           gboolean            enable_mnemonics)
{
  return GDK_KEY_MATCH_NONE;
}

static guint
gtk_never_trigger_hash (GtkShortcutTrigger *trigger)
{
  return GTK_SHORTCUT_TRIGGER_HASH_NEVER;
}

static int
gtk_never_trigger_compare (GtkShortcutTrigger  *trigger1,
                           GtkShortcutTrigger  *trigger2)
{
  return 0;
}

static void
gtk_never_trigger_print (GtkShortcutTrigger *trigger,
                         GString            *string)
{
  g_string_append (string, "never");
}

static gboolean
gtk_never_trigger_print_label (GtkShortcutTrigger *trigger,
                               GdkDisplay         *display,
                               GString            *string)
{
  return FALSE;
}

static void
gtk_never_trigger_class_init (GtkNeverTriggerClass *klass)
{
  GtkShortcutTriggerClass *trigger_class = GTK_SHORTCUT_TRIGGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructor = gtk_never_trigger_constructor;
  gobject_class->finalize = gtk_never_trigger_finalize;

  trigger_class->trigger = gtk_never_trigger_trigger;
  trigger_class->hash = gtk_never_trigger_hash;
  trigger_class->compare = gtk_never_trigger_compare;
  trigger_class->print = gtk_never_trigger_print;
  trigger_class->print_label = gtk_never_trigger_print_label;
}

static void
gtk_never_trigger_init (GtkNeverTrigger *self)
{
  /* We are the singleton being constructed */
  g_assert (!never_singleton);
}

/**
 * gtk_never_trigger_get:
 *
 * Gets the never trigger.
 *
 * This is a singleton for a trigger that never triggers.
 * Use this trigger instead of %NULL because it implements
 * all virtual functions.
 *
 * Returns: (type GtkNeverTrigger) (transfer none): The never trigger
 */
GtkShortcutTrigger *
gtk_never_trigger_get (void)
{
  if (G_UNLIKELY (never_singleton == NULL))
    {
      GtkShortcutTrigger *never = g_object_new (GTK_TYPE_NEVER_TRIGGER, NULL);
      g_assert (never == never_singleton);
      g_object_unref (never);
    }

  return never_singleton;
}

/* }}} */
/* {{{ Keyval trigger */

struct _GtkKeyvalTrigger
{
  GtkShortcutTrigger parent_instance;

  guint keyval;
  GdkModifierType modifiers;
};

struct _GtkKeyvalTriggerClass
{
  GtkShortcutTriggerClass parent_class;
};

G_DEFINE_TYPE (GtkKeyvalTrigger, gtk_keyval_trigger, GTK_TYPE_SHORTCUT_TRIGGER)

enum
{
  KEYVAL_PROP_KEYVAL = 1,
  KEYVAL_PROP_MODIFIERS,
  KEYVAL_N_PROPS
};

static GParamSpec *keyval_props[KEYVAL_N_PROPS];

static GdkKeyMatch
gtk_keyval_trigger_trigger (GtkShortcutTrigger *trigger,
                            GdkEvent           *event,
                            gboolean            enable_mnemonics)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (trigger);

  if (gdk_event_get_event_type (event) != GDK_KEY_PRESS)
    return GDK_KEY_MATCH_NONE;

  return gdk_key_event_matches (event, self->keyval, self->modifiers);
}

static guint
gtk_keyval_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (trigger);

  return (self->modifiers << 24)
       | (self->modifiers >> 8)
       | (self->keyval << 16)
       | GTK_SHORTCUT_TRIGGER_HASH_KEYVAL;
}

static int
gtk_keyval_trigger_compare (GtkShortcutTrigger  *trigger1,
                            GtkShortcutTrigger  *trigger2)
{
  GtkKeyvalTrigger *self1 = GTK_KEYVAL_TRIGGER (trigger1);
  GtkKeyvalTrigger *self2 = GTK_KEYVAL_TRIGGER (trigger2);

  if (self1->modifiers != self2->modifiers)
    return self2->modifiers - self1->modifiers;

  return self1->keyval - self2->keyval;
}

static void
gtk_keyval_trigger_print (GtkShortcutTrigger *trigger,
                          GString            *string)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (trigger);
  char *accelerator_name;

  accelerator_name = gtk_accelerator_name (self->keyval, self->modifiers);
  g_string_append (string, accelerator_name);
  g_free (accelerator_name);
}

static gboolean
gtk_keyval_trigger_print_label (GtkShortcutTrigger *trigger,
                                GdkDisplay         *display,
                                GString            *string)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (trigger);

  gtk_accelerator_print_label (string, self->keyval, self->modifiers);

  return TRUE;
}

static void
gtk_keyval_trigger_set_property (GObject      *gobject,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (gobject);

  switch (prop_id)
    {
    case KEYVAL_PROP_KEYVAL:
      {
        guint keyval = g_value_get_uint (value);

        /* We store keyvals as lower key */
        if (keyval == GDK_KEY_ISO_Left_Tab)
          self->keyval = GDK_KEY_Tab;
        else
          self->keyval = gdk_keyval_to_lower (keyval);
      }
      break;

    case KEYVAL_PROP_MODIFIERS:
      self->modifiers = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_keyval_trigger_get_property (GObject    *gobject,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkKeyvalTrigger *self = GTK_KEYVAL_TRIGGER (gobject);

  switch (prop_id)
    {
    case KEYVAL_PROP_KEYVAL:
      g_value_set_uint (value, self->keyval);
      break;

    case KEYVAL_PROP_MODIFIERS:
      g_value_set_flags (value, self->modifiers);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_keyval_trigger_class_init (GtkKeyvalTriggerClass *klass)
{
  GtkShortcutTriggerClass *trigger_class = GTK_SHORTCUT_TRIGGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_keyval_trigger_set_property;
  gobject_class->get_property = gtk_keyval_trigger_get_property;

  trigger_class->trigger = gtk_keyval_trigger_trigger;
  trigger_class->hash = gtk_keyval_trigger_hash;
  trigger_class->compare = gtk_keyval_trigger_compare;
  trigger_class->print = gtk_keyval_trigger_print;
  trigger_class->print_label = gtk_keyval_trigger_print_label;

  /**
   * GtkKeyvalTrigger:keyval:
   *
   * The key value for the trigger.
   */
  keyval_props[KEYVAL_PROP_KEYVAL] =
    g_param_spec_uint ("keyval", NULL, NULL,
                       0, G_MAXINT,
                       0,
                       G_PARAM_STATIC_NAME |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_READWRITE);

  /**
   * GtkKeyvalTrigger:modifiers:
   *
   * The key modifiers for the trigger.
   */
  keyval_props[KEYVAL_PROP_MODIFIERS] =
    g_param_spec_flags ("modifiers", NULL, NULL,
                        GDK_TYPE_MODIFIER_TYPE,
                        GDK_NO_MODIFIER_MASK,
                        G_PARAM_STATIC_NAME |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, KEYVAL_N_PROPS, keyval_props);
}

static void
gtk_keyval_trigger_init (GtkKeyvalTrigger *self)
{
}

/**
 * gtk_keyval_trigger_new:
 * @keyval: The keyval to trigger for
 * @modifiers: the modifiers that need to be present
 *
 * Creates a `GtkShortcutTrigger` that will trigger whenever
 * the key with the given @keyval and @modifiers is pressed.
 *
 * Returns: A new `GtkShortcutTrigger`
 */
GtkShortcutTrigger *
gtk_keyval_trigger_new (guint           keyval,
                        GdkModifierType modifiers)
{
  return g_object_new (GTK_TYPE_KEYVAL_TRIGGER,
                       "keyval", keyval,
                       "modifiers", modifiers,
                       NULL);
}

/**
 * gtk_keyval_trigger_get_modifiers:
 * @self: a keyval `GtkShortcutTrigger`
 *
 * Gets the modifiers that must be present to succeed
 * triggering @self.
 *
 * Returns: the modifiers
 */
GdkModifierType
gtk_keyval_trigger_get_modifiers (GtkKeyvalTrigger *self)
{
  g_return_val_if_fail (GTK_IS_KEYVAL_TRIGGER (self), 0);

  return self->modifiers;
}

/**
 * gtk_keyval_trigger_get_keyval:
 * @self: a keyval `GtkShortcutTrigger`
 *
 * Gets the keyval that must be pressed to succeed
 * triggering @self.
 *
 * Returns: the keyval
 */
guint
gtk_keyval_trigger_get_keyval (GtkKeyvalTrigger *self)
{
  g_return_val_if_fail (GTK_IS_KEYVAL_TRIGGER (self), 0);

  return self->keyval;
}

/* }}} */
/* {{{ Mnemonic trigger */

struct _GtkMnemonicTrigger
{
  GtkShortcutTrigger parent_instance;

  guint keyval;
};

struct _GtkMnemonicTriggerClass
{
  GtkShortcutTriggerClass parent_class;
};

G_DEFINE_TYPE (GtkMnemonicTrigger, gtk_mnemonic_trigger, GTK_TYPE_SHORTCUT_TRIGGER)

enum
{
  MNEMONIC_PROP_KEYVAL = 1,
  MNEMONIC_N_PROPS
};

static GParamSpec *mnemonic_props[MNEMONIC_N_PROPS];

static GdkKeyMatch
gtk_mnemonic_trigger_trigger (GtkShortcutTrigger *trigger,
                              GdkEvent           *event,
                              gboolean            enable_mnemonics)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (trigger);
  guint keyval;

  if (!enable_mnemonics)
    return GDK_KEY_MATCH_NONE;

  if (gdk_event_get_event_type (event) != GDK_KEY_PRESS)
    return GDK_KEY_MATCH_NONE;

  /* XXX: This needs to deal with groups */
  keyval = gdk_key_event_get_keyval (event);

  if (keyval == GDK_KEY_ISO_Left_Tab)
    keyval = GDK_KEY_Tab;
  else
    keyval = gdk_keyval_to_lower (keyval);

  if (keyval != self->keyval)
    return GDK_KEY_MATCH_NONE;

  return GDK_KEY_MATCH_EXACT;
}

static guint
gtk_mnemonic_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (trigger);

  return (self->keyval << 8)
       | GTK_SHORTCUT_TRIGGER_HASH_MNEMONIC;
}

static int
gtk_mnemonic_trigger_compare (GtkShortcutTrigger  *trigger1,
                              GtkShortcutTrigger  *trigger2)
{
  GtkMnemonicTrigger *self1 = GTK_MNEMONIC_TRIGGER (trigger1);
  GtkMnemonicTrigger *self2 = GTK_MNEMONIC_TRIGGER (trigger2);

  return self1->keyval - self2->keyval;
}

static void
gtk_mnemonic_trigger_print (GtkShortcutTrigger *trigger,
                            GString            *string)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (trigger);
  const char *keyval_str;

  keyval_str = gdk_keyval_name (self->keyval);
  if (keyval_str == NULL)
    keyval_str = "???";

  g_string_append (string, "<Mnemonic>");
  g_string_append (string, keyval_str);
}

static gboolean
gtk_mnemonic_trigger_print_label (GtkShortcutTrigger *trigger,
                                  GdkDisplay         *display,
                                  GString            *string)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (trigger);
  const char *keyval_str;

  keyval_str = gdk_keyval_name (self->keyval);
  if (keyval_str == NULL)
    return FALSE;

  g_string_append (string, keyval_str);

  return TRUE;
}

static void
gtk_mnemonic_trigger_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (gobject);

  switch (prop_id)
    {
    case MNEMONIC_PROP_KEYVAL:
      {
        guint keyval = g_value_get_uint (value);

        /* We store keyvals as lower key */
        if (keyval == GDK_KEY_ISO_Left_Tab)
          self->keyval = GDK_KEY_Tab;
        else
          self->keyval = gdk_keyval_to_lower (keyval);
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_mnemonic_trigger_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkMnemonicTrigger *self = GTK_MNEMONIC_TRIGGER (gobject);

  switch (prop_id)
    {
    case MNEMONIC_PROP_KEYVAL:
      g_value_set_uint (value, self->keyval);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_mnemonic_trigger_class_init (GtkMnemonicTriggerClass *klass)
{
  GtkShortcutTriggerClass *trigger_class = GTK_SHORTCUT_TRIGGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_mnemonic_trigger_set_property;
  gobject_class->get_property = gtk_mnemonic_trigger_get_property;

  trigger_class->trigger = gtk_mnemonic_trigger_trigger;
  trigger_class->hash = gtk_mnemonic_trigger_hash;
  trigger_class->compare = gtk_mnemonic_trigger_compare;
  trigger_class->print = gtk_mnemonic_trigger_print;
  trigger_class->print_label = gtk_mnemonic_trigger_print_label;

  /**
   * GtkMnemonicTrigger:keyval:
   *
   * The key value for the trigger.
   */
  mnemonic_props[MNEMONIC_PROP_KEYVAL] =
    g_param_spec_uint ("keyval", NULL, NULL,
                       0, G_MAXINT,
                       0,
                       G_PARAM_STATIC_NAME |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, MNEMONIC_N_PROPS, mnemonic_props);
}

static void
gtk_mnemonic_trigger_init (GtkMnemonicTrigger *self)
{
}

/**
 * gtk_mnemonic_trigger_new:
 * @keyval: The keyval to trigger for
 *
 * Creates a `GtkShortcutTrigger` that will trigger whenever the key with
 * the given @keyval is pressed and mnemonics have been activated.
 *
 * Mnemonics are activated by calling code when a key event with the right
 * modifiers is detected.
 *
 * Returns: (transfer full) (type GtkMnemonicTrigger): A new `GtkShortcutTrigger`
 */
GtkShortcutTrigger *
gtk_mnemonic_trigger_new (guint keyval)
{
  return g_object_new (GTK_TYPE_MNEMONIC_TRIGGER,
                       "keyval", keyval,
                       NULL);
}

/**
 * gtk_mnemonic_trigger_get_keyval:
 * @self: a mnemonic `GtkShortcutTrigger`
 *
 * Gets the keyval that must be pressed to succeed triggering @self.
 *
 * Returns: the keyval
 */
guint
gtk_mnemonic_trigger_get_keyval (GtkMnemonicTrigger *self)
{
  g_return_val_if_fail (GTK_IS_MNEMONIC_TRIGGER (self), 0);

  return self->keyval;
}

/* }}} */
/* {{{ Alternative trigger */

struct _GtkAlternativeTrigger
{
  GtkShortcutTrigger parent_instance;

  GtkShortcutTrigger **triggers;
  unsigned int n_triggers;
};

struct _GtkAlternativeTriggerClass
{
  GtkShortcutTriggerClass parent_class;
};

static GType
gtk_alternative_trigger_get_item_type (GListModel *model)
{
  return GTK_TYPE_SHORTCUT_TRIGGER;
}

static unsigned int
gtk_alternative_trigger_get_n_items (GListModel *model)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (model);

  return self->n_triggers;
}

static gpointer
gtk_alternative_trigger_get_item (GListModel   *model,
                                  unsigned int  position)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (model);

  if (position < self->n_triggers)
    return g_object_ref (self->triggers[position]);
  else
    return NULL;
}

static void
gtk_alternative_trigger_list_model_init (GListModelInterface *iface)
{
  iface->get_item_type = gtk_alternative_trigger_get_item_type;
  iface->get_n_items = gtk_alternative_trigger_get_n_items;
  iface->get_item = gtk_alternative_trigger_get_item;
}

G_DEFINE_TYPE_WITH_CODE (GtkAlternativeTrigger, gtk_alternative_trigger, GTK_TYPE_SHORTCUT_TRIGGER,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, gtk_alternative_trigger_list_model_init))

enum
{
  ALTERNATIVE_PROP_FIRST = 1,
  ALTERNATIVE_PROP_SECOND,
  ALTERNATIVE_N_PROPS
};

static GParamSpec *alternative_props[ALTERNATIVE_N_PROPS];

static void
gtk_alternative_trigger_dispose (GObject *gobject)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (gobject);

  for (unsigned int i = 0; i < self->n_triggers; i++)
    g_object_unref (self->triggers[i]);

  g_clear_pointer (&self->triggers, g_free);
  self->n_triggers = 0;

  G_OBJECT_CLASS (gtk_alternative_trigger_parent_class)->dispose (gobject);
}

static GdkKeyMatch
gtk_alternative_trigger_trigger (GtkShortcutTrigger *trigger,
                                 GdkEvent           *event,
                                 gboolean            enable_mnemonics)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (trigger);
  GdkKeyMatch match = GDK_KEY_MATCH_NONE;

  for (unsigned int i = 0; i < self->n_triggers; i++)
    match = MAX (match, gtk_shortcut_trigger_trigger (self->triggers[i], event, enable_mnemonics));

  return match;
}

static guint
gtk_alternative_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (trigger);
  guint result = 0;

  for (unsigned int i = 0; i < self->n_triggers; i++)
    {
      result |= gtk_shortcut_trigger_hash (self->triggers[i]);
      result = result << 5 | result >> 27;
    }

  return result | GTK_SHORTCUT_TRIGGER_HASH_ALTERNATIVE;
}

static int
gtk_alternative_trigger_compare (GtkShortcutTrigger  *trigger1,
                                 GtkShortcutTrigger  *trigger2)
{
  GtkAlternativeTrigger *self1 = GTK_ALTERNATIVE_TRIGGER (trigger1);
  GtkAlternativeTrigger *self2 = GTK_ALTERNATIVE_TRIGGER (trigger2);
  int cmp;

  if (self1->n_triggers != self2->n_triggers)
    return self2->n_triggers - self1->n_triggers;

  for (unsigned int i = 0; i < self1->n_triggers; i++)
    {
      cmp = gtk_shortcut_trigger_compare (self1->triggers[i], self2->triggers[i]);
      if (cmp != 0)
        return cmp;
    }

  return 0;
}

static void
gtk_alternative_trigger_print (GtkShortcutTrigger *trigger,
                               GString            *string)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (trigger);

  for (unsigned int i = 0; i < self->n_triggers; i++)
    {
      if (i > 0)
        g_string_append (string, "|");
      gtk_shortcut_trigger_print (self->triggers[i], string);
    }
}

static gboolean
gtk_alternative_trigger_print_label (GtkShortcutTrigger *trigger,
                                     GdkDisplay         *display,
                                     GString            *string)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (trigger);
  gboolean retval = TRUE;

  for (unsigned int i = 0; i < self->n_triggers; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      if (!gtk_shortcut_trigger_print_label (self->triggers[i], display, string))
        {
          if (i > 0)
            g_string_truncate (string, string->len - 2);
          retval = FALSE;
        }
    }

  return retval;
}

static void
gtk_alternative_trigger_set_property (GObject      *gobject,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (gobject);
  GtkShortcutTrigger *trigger;

  switch (prop_id)
    {
    case ALTERNATIVE_PROP_FIRST:
      trigger = g_value_get_object (value);
      if (!trigger)
        trigger = gtk_never_trigger_get ();
      g_set_object (&self->triggers[0], trigger);
      break;

    case ALTERNATIVE_PROP_SECOND:
      trigger = g_value_get_object (value);
      if (!trigger)
        trigger = gtk_never_trigger_get ();
      g_set_object (&self->triggers[1], trigger);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_alternative_trigger_get_property (GObject    *gobject,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  GtkAlternativeTrigger *self = GTK_ALTERNATIVE_TRIGGER (gobject);

  switch (prop_id)
    {
    case ALTERNATIVE_PROP_FIRST:
      g_value_set_object (value, self->triggers[0]);
      break;

    case ALTERNATIVE_PROP_SECOND:
      g_value_set_object (value, self->triggers[1]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    }
}

static void
gtk_alternative_trigger_class_init (GtkAlternativeTriggerClass *klass)
{
  GtkShortcutTriggerClass *trigger_class = GTK_SHORTCUT_TRIGGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_alternative_trigger_set_property;
  gobject_class->get_property = gtk_alternative_trigger_get_property;
  gobject_class->dispose = gtk_alternative_trigger_dispose;

  trigger_class->trigger = gtk_alternative_trigger_trigger;
  trigger_class->hash = gtk_alternative_trigger_hash;
  trigger_class->compare = gtk_alternative_trigger_compare;
  trigger_class->print = gtk_alternative_trigger_print;
  trigger_class->print_label = gtk_alternative_trigger_print_label;

  /**
   * GtkAlternativeTrigger:first:
   *
   * The first `GtkShortcutTrigger` to check.
   */
  alternative_props[ALTERNATIVE_PROP_FIRST] =
    g_param_spec_object ("first", NULL, NULL,
                         GTK_TYPE_SHORTCUT_TRIGGER,
                         G_PARAM_STATIC_NAME |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE);

  /**
   * GtkAlternativeTrigger:second:
   *
   * The second `GtkShortcutTrigger` to check.
   */
  alternative_props[ALTERNATIVE_PROP_SECOND] =
    g_param_spec_object ("second", NULL, NULL,
                         GTK_TYPE_SHORTCUT_TRIGGER,
                         G_PARAM_STATIC_NAME |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_READWRITE);

  g_object_class_install_properties (gobject_class, ALTERNATIVE_N_PROPS, alternative_props);
}

static void
gtk_alternative_trigger_init (GtkAlternativeTrigger *self)
{
  self->n_triggers = 2;
  self->triggers = g_new (GtkShortcutTrigger *, 2);
  self->triggers[0] = g_object_ref (gtk_never_trigger_get ());
  self->triggers[1] = g_object_ref (gtk_never_trigger_get ());
}

/**
 * gtk_alternative_trigger_new:
 * @first: (transfer full): The first trigger that may trigger
 * @second: (transfer full): The second trigger that may trigger
 *
 * Creates a `GtkShortcutTrigger` that will trigger whenever
 * either of the two given triggers gets triggered.
 *
 * Note that nesting is allowed, so if you want more than two
 * alternative, create a new alternative trigger for each option.
 *
 * Returns: a new `GtkShortcutTrigger`
 */
GtkShortcutTrigger *
gtk_alternative_trigger_new (GtkShortcutTrigger *first,
                             GtkShortcutTrigger *second)
{
  GtkShortcutTrigger *res;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (first), NULL);
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (second), NULL);

  res = g_object_new (GTK_TYPE_ALTERNATIVE_TRIGGER,
                      "first", first,
                      "second", second,
                      NULL);

  g_object_unref (first);
  g_object_unref (second);

  return res;
}

/**
 * gtk_alternative_trigger_get_first:
 * @self: an alternative `GtkShortcutTrigger`
 *
 * Gets the first of the two alternative triggers that may
 * trigger @self.
 *
 * [method@Gtk.AlternativeTrigger.get_second] will return
 * the other one.
 *
 * Returns: (transfer none): the first alternative trigger
 */
GtkShortcutTrigger *
gtk_alternative_trigger_get_first (GtkAlternativeTrigger *self)
{
  g_return_val_if_fail (GTK_IS_ALTERNATIVE_TRIGGER (self), NULL);

  return self->triggers[0];
}

/**
 * gtk_alternative_trigger_get_second:
 * @self: an alternative `GtkShortcutTrigger`
 *
 * Gets the second of the two alternative triggers that may
 * trigger @self.
 *
 * [method@Gtk.AlternativeTrigger.get_first] will return
 * the other one.
 *
 * Returns: (transfer none): the second alternative trigger
 */
GtkShortcutTrigger *
gtk_alternative_trigger_get_second (GtkAlternativeTrigger *self)
{
  g_return_val_if_fail (GTK_IS_ALTERNATIVE_TRIGGER (self), NULL);

  return self->triggers[1];
}

/**
 * gtk_alternative_trigger_newv:
 * @triggers: (array length=n_triggers) (transfer full): the triggers
 * @n_triggers: the number of triggers
 *
 * Creates a `GtkShortcutTrigger` that will trigger whenever
 * any of the given triggers gets triggered.
 *
 * Returns: a new `GtkShortcutTrigger`
 *
 * Since: 4.24
 */
GtkShortcutTrigger *
gtk_alternative_trigger_newv (GtkShortcutTrigger **triggers,
                              size_t               n_triggers)
{
  GtkAlternativeTrigger *self;

  self = g_object_new (GTK_TYPE_ALTERNATIVE_TRIGGER, NULL);

  g_object_unref (self->triggers[0]);
  g_object_unref (self->triggers[1]);
  g_free (self->triggers);

  self->n_triggers = MAX (n_triggers, 2);
  self->triggers = g_new (GtkShortcutTrigger *, self->n_triggers);

  for (unsigned int i = 0; i < self->n_triggers; i++)
    {
      if (i < n_triggers)
        self->triggers[i] = triggers[i];
      else
        self->triggers[i] = g_object_ref (gtk_never_trigger_get ());
    }

  return GTK_SHORTCUT_TRIGGER (self);
}

/* }}} */
/* {{{ Convenience API */

/**
 * gtk_shortcut_trigger_create_with_aliases:
 * @keyval: The keyval to trigger for
 * @modifiers: the modifiers that need to be present
 *
 * Creates a shortcut trigger that will trigger for
 * any alias of the given key.
 *
 * Returns: (transfer full): a new `GtkShortcutTrigger`
 *
 * Since: 4.24
 */
GtkShortcutTrigger *
gtk_shortcut_trigger_create_with_aliases (unsigned int    keyval,
                                          GdkModifierType modifiers)
{
  const unsigned int *keys;
  unsigned int n_keys;
  GtkShortcutTrigger **triggers;

  keys = gdk_keyval_get_aliases (keyval, &n_keys);

  if (n_keys < 2)
    return gtk_keyval_trigger_new (keyval, modifiers);

  triggers = g_newa (GtkShortcutTrigger *, n_keys);
  for (unsigned int i = 0; i < n_keys; i++)
    triggers[i] = gtk_keyval_trigger_new (keys[i], modifiers);

  return gtk_alternative_trigger_newv (triggers, n_keys);
}

/**
 * gtk_shortcut_trigger_create_for_menu:
 *
 * Creates a shortcut trigger that will trigger for
 * the usual key combinations that trigger a context
 * menu, such as <kbd>Menu</kbd> or
 * <kbd>Shift</kbd>+<kbd>F10</kbd>.
 *
 * Returns: (transfer full): a new `GtkShortcutTrigger`
 *
 * Since: 4.24
 */
GtkShortcutTrigger *
gtk_shortcut_trigger_create_for_menu (void)
{
  static GtkShortcutTrigger *menu_trigger = NULL;

  if (menu_trigger == NULL)
    {
      const unsigned int *keys;
      unsigned int n_keys;
      GtkShortcutTrigger **triggers;

      keys = gdk_keyval_get_aliases (GDK_KEY_Menu, &n_keys);

      triggers = g_newa (GtkShortcutTrigger *, n_keys + 1);
      for (unsigned int i = 0; i < n_keys; i++)
        triggers[i] = gtk_keyval_trigger_new (keys[i], 0);

      triggers[n_keys] = gtk_keyval_trigger_new (GDK_KEY_F10, GDK_SHIFT_MASK);

      menu_trigger = gtk_alternative_trigger_newv (triggers, n_keys + 1);
    }

  return g_object_ref (menu_trigger);
}

/* }}} */

/* vim:set foldmethod=marker: */
