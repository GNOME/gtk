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
 * SECTION:gtkshortcuttrigger
 * @Title: GtkShortcutTrigger
 * @Short_description: Triggers to track if shortcuts should be activated
 * @See_also: #GtkShortcut
 *
 * #GtkShortcutTrigger is the object used to track if a #GtkShortcut should be
 * activated. For this purpose, gtk_shortcut_trigger_trigger() can be called
 * on a #GdkEvent.
 *
 * #GtkShortcutTriggers contain functions that allow easy presentation to end
 * users as well as being printed for debugging.
 *
 * All #GtkShortcutTriggers are immutable, you can only specify their properties
 * during construction. If you want to change a trigger, you have to replace it
 * with a new one.
 */

#include "config.h"

#include "gtkshortcuttrigger.h"

#include "gtkaccelgroupprivate.h"
#include "gtkprivate.h"

typedef struct _GtkShortcutTriggerClass GtkShortcutTriggerClass;

#define GTK_IS_SHORTCUT_TRIGGER_TYPE(trigger,type) (GTK_IS_SHORTCUT_TRIGGER (trigger) && (trigger)->trigger_class->trigger_type == (type))

struct _GtkShortcutTrigger
{
  const GtkShortcutTriggerClass *trigger_class;

  gatomicrefcount ref_count;
};

struct _GtkShortcutTriggerClass
{
  GtkShortcutTriggerType trigger_type;
  gsize struct_size;
  const char *type_name;

  void            (* finalize)    (GtkShortcutTrigger  *trigger);
  GtkShortcutTriggerMatch
                  (* trigger)     (GtkShortcutTrigger  *trigger,
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

G_DEFINE_BOXED_TYPE (GtkShortcutTrigger, gtk_shortcut_trigger,
                     gtk_shortcut_trigger_ref,
                     gtk_shortcut_trigger_unref)

static void
gtk_shortcut_trigger_finalize (GtkShortcutTrigger *self)
{
  self->trigger_class->finalize (self);

  g_free (self);
}

/*< private >
 * gtk_shortcut_trigger_new:
 * @trigger_class: class structure for this trigger
 *
 * Returns: (transfer full): the newly created #GtkShortcutTrigger
 */
static GtkShortcutTrigger *
gtk_shortcut_trigger_new (const GtkShortcutTriggerClass *trigger_class)
{
  GtkShortcutTrigger *self;

  g_return_val_if_fail (trigger_class != NULL, NULL);

  self = g_malloc0 (trigger_class->struct_size);
  g_atomic_ref_count_init (&self->ref_count);

  self->trigger_class = trigger_class;

  return self;
}

/**
 * gtk_shortcut_trigger_ref:
 * @self: a #GtkShortcutTrigger
 *
 * Acquires a reference on the given #GtkShortcutTrigger.
 *
 * Returns: (transfer full): the #GtkShortcutTrigger with
 *    an additional reference
 */
GtkShortcutTrigger *
gtk_shortcut_trigger_ref (GtkShortcutTrigger *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), NULL);

  g_atomic_ref_count_inc (&self->ref_count);

  return self;
}

/**
 * gtk_shortcut_trigger_unref:
 * @self: (transfer full): a #GtkShortcutTrigger
 *
 * Releases a reference on the given #GtkShortcutTrigger.
 *
 * If the reference was the last, the resources associated
 * to the trigger are freed.
 */
void
gtk_shortcut_trigger_unref (GtkShortcutTrigger *self)
{
  g_return_if_fail (GTK_IS_SHORTCUT_TRIGGER (self));

  if (g_atomic_ref_count_dec (&self->ref_count))
    gtk_shortcut_trigger_finalize (self);
}

/**
 * gtk_shortcut_trigger_get_trigger_type:
 * @self: a #GtkShortcutTrigger
 *
 * Returns the type of the @trigger.
 *
 * Returns: the type of the #GtkShortcutTrigger
 */
GtkShortcutTriggerType
gtk_shortcut_trigger_get_trigger_type (GtkShortcutTrigger *self)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), GTK_SHORTCUT_TRIGGER_NEVER);

  return self->trigger_class->trigger_type;
}

/**
 * gtk_shortcut_trigger_trigger:
 * @self: a #GtkShortcutTrigger
 * @event: the event to check
 * @enable_mnemonics: %TRUE if mnemonics should trigger. Usually the
 *     value of this property is determined by checking that the passed
 *     in @event is a Key event and has the right modifiers set.
 *
 * Checks if the given @event triggers @self.
 *
 * Returns: Whether the event triggered the shortcut
 **/
GtkShortcutTriggerMatch
gtk_shortcut_trigger_trigger (GtkShortcutTrigger *self,
                              GdkEvent           *event,
                              gboolean            enable_mnemonics)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), GTK_SHORTCUT_TRIGGER_MATCH_NONE);

  return self->trigger_class->trigger (self, event, enable_mnemonics);
}

/**
 * gtk_shortcut_trigger_parse_string:
 * @string: the string to parse
 *
 * Tries to parse the given string into a trigger. On success,
 * the parsed trigger is returned. When parsing failed, %NULL is
 * returned.
 *
 * FIXME: Document the supported format here once we've figured
 * it out.
 * For now, this function only supports gtk_accelerator_parse() and
 * can only return a trigger of type %GTK_SHORTCUT_TRIGGER_KEYVAL.
 *
 * Returns: a new #GtkShortcutTrigger or %NULL on error
 **/
GtkShortcutTrigger *
gtk_shortcut_trigger_parse_string (const char *string)
{
  GdkModifierType modifiers;
  guint keyval;

  g_return_val_if_fail (string != NULL, NULL);

  if (gtk_accelerator_parse (string, &keyval, &modifiers))
    return gtk_keyval_trigger_new (keyval, modifiers);

  return NULL;
}

/**
 * gtk_shortcut_trigger_to_string:
 * @self: a #GtkShortcutTrigger
 *
 * Prints the given trigger into a human-readable string.
 * This is a small wrapper around gdk_content_formats_print()
 * to help when debugging.
 *
 * Returns: (transfer full): a new string
 **/
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
 * @self: a #GtkShortcutTrigger
 * @string: a #GString to print into
 *
 * Prints the given trigger into a string for the developer.
 * This is meant for debugging and logging.
 *
 * The form of the representation may change at any time
 * and is not guaranteed to stay identical.
 **/
void
gtk_shortcut_trigger_print (GtkShortcutTrigger *self,
                            GString            *string)
{
  g_return_if_fail (GTK_IS_SHORTCUT_TRIGGER (self));
  g_return_if_fail (string != NULL);

  return self->trigger_class->print (self, string);
}

/**
 * gtk_shortcut_trigger_to_label:
 * @self: a #GtkShortcutTrigger
 * @display: #GdkDisplay to print for
 *
 * Gets textual representation for the given trigger. This
 * function is returning a translated string for presentation
 * to end users for example in menu items or in help texts.
 *
 * The @display in use may influence the resulting string in
 * various forms, such as resolving hardware keycodes or by
 * causing display-specific modifier names.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 *
 * Returns: (transfer full): a new string
 **/
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
 * @self: a #GtkShortcutTrigger
 * @display: #GdkDisplay to print for
 * @string: a #GString to print into
 *
 * Prints the given trigger into a string. This function is
 * returning a translated string for presentation to end users
 * for example in menu items or in help texts.
 *
 * The @display in use may influence the resulting string in
 * various forms, such as resolving hardware keycodes or by
 * causing display-specific modifier names.
 *
 * The form of the representation may change at any time and is
 * not guaranteed to stay identical.
 *
 * Returns: %TRUE if something was printed or %FALSE if the
 *     trigger did not have a textual representation suitable
 *     for end users.
 **/
gboolean
gtk_shortcut_trigger_print_label (GtkShortcutTrigger *self,
                                  GdkDisplay         *display,
                                  GString            *string)
{
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (self), FALSE);
  g_return_val_if_fail (GDK_IS_DISPLAY (display), FALSE);
  g_return_val_if_fail (string != NULL, FALSE);

  return self->trigger_class->print_label (self, display, string);
}

/**
 * gtk_shortcut_trigger_hash:
 * @trigger: (type GtkShortcutTrigger): a #GtkShortcutTrigger
 *
 * Generates a hash value for a #GtkShortcutTrigger.
 *
 * The output of this function is guaranteed to be the same for a given
 * value only per-process.  It may change between different processor
 * architectures or even different versions of GTK.  Do not use this
 * function as a basis for building protocols or file formats.
 *
 * The types of @trigger is #gconstpointer only to allow use of this
 * function with #GHashTable. They must each be a #GtkShortcutTrigger.
 *
 * Returns: a hash value corresponding to @trigger
 **/
guint
gtk_shortcut_trigger_hash (gconstpointer trigger)
{
  GtkShortcutTrigger *t = (GtkShortcutTrigger *) trigger;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (trigger), 0);

  return t->trigger_class->hash (t);
}

/**
 * gtk_shortcut_trigger_equal:
 * @trigger1: (type GtkShortcutTrigger): a #GtkShortcutTrigger
 * @trigger2: (type GtkShortcutTrigger): a #GtkShortcutTrigger
 *
 * Checks if @trigger1 and @trigger2 trigger under the same conditions.
 *
 * The types of @one and @two are #gconstpointer only to allow use of this
 * function with #GHashTable. They must each be a #GtkShortcutTrigger.
 *
 * Returns: %TRUE if @trigger1 and @trigger2 are equal
 **/
gboolean
gtk_shortcut_trigger_equal (gconstpointer trigger1,
                            gconstpointer trigger2)
{
  return gtk_shortcut_trigger_compare (trigger1, trigger2) == 0;
}

/**
 * gtk_shortcut_trigger_compare:
 * @trigger1: (type GtkShortcutTrigger): a #GtkShortcutTrigger
 * @trigger2: (type GtkShortcutTrigger): a #GtkShortcutTrigger
 *
 * 
 * The types of @one and @two are #gconstpointer only to allow use of this
 * function as a #GCompareFunc. They must each be a #GtkShortcutTrigger.
 *
 * Returns: An integer less than, equal to, or greater than zero if
 *     @trigger1 is found, respectively, to be less than, to match,
 *     or be greater than @trigger2.
 **/
gint
gtk_shortcut_trigger_compare (gconstpointer trigger1,
                              gconstpointer trigger2)
{
  GtkShortcutTrigger *t1 = (GtkShortcutTrigger *) trigger1;
  GtkShortcutTrigger *t2 = (GtkShortcutTrigger *) trigger2;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (trigger1), -1);
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (trigger2), 1);

  if (t1->trigger_class != t2->trigger_class)
    return t1->trigger_class->trigger_type - t2->trigger_class->trigger_type;

  return t1->trigger_class->compare (t1, t2);
}

/*** GTK_SHORTCUT_TRIGGER_NEVER ***/

typedef struct _GtkNeverTrigger GtkNeverTrigger;

struct _GtkNeverTrigger
{
  GtkShortcutTrigger trigger;

  guint never;
  GdkModifierType modifiers;
};

static void
gtk_never_trigger_finalize (GtkShortcutTrigger *trigger)
{
  g_assert_not_reached ();
}

static GtkShortcutTriggerMatch
gtk_never_trigger_trigger (GtkShortcutTrigger *trigger,
                           GdkEvent           *event,
                           gboolean            enable_mnemonics)
{
  return GTK_SHORTCUT_TRIGGER_MATCH_NONE;
}

static guint
gtk_never_trigger_hash (GtkShortcutTrigger *trigger)
{
  return GTK_SHORTCUT_TRIGGER_NEVER;
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
  g_string_append (string, "<never>");
}

static gboolean
gtk_never_trigger_print_label (GtkShortcutTrigger *trigger,
                               GdkDisplay         *display,
                               GString            *string)
{
  return FALSE;
}

static const GtkShortcutTriggerClass GTK_NEVER_TRIGGER_CLASS = {
  GTK_SHORTCUT_TRIGGER_NEVER,
  sizeof (GtkNeverTrigger),
  "GtkNeverTrigger",
  gtk_never_trigger_finalize,
  gtk_never_trigger_trigger,
  gtk_never_trigger_hash,
  gtk_never_trigger_compare,
  gtk_never_trigger_print,
  gtk_never_trigger_print_label
};

static GtkNeverTrigger never = { { &GTK_NEVER_TRIGGER_CLASS, 1 } };

/**
 * gtk_never_trigger_get:
 *
 * Gets the never trigger. This is a singleton for a trigger
 * that never triggers. Use this trigger instead of %NULL
 * because it implements all virtual functions.
 *
 * Returns: (transfer none): The never trigger
 */
GtkShortcutTrigger *
gtk_never_trigger_get (void)
{
  return &never.trigger;
}

/*** GTK_KEYVAL_TRIGGER ***/

typedef struct _GtkKeyvalTrigger GtkKeyvalTrigger;

struct _GtkKeyvalTrigger
{
  GtkShortcutTrigger trigger;

  guint keyval;
  GdkModifierType modifiers;
};

static void
gtk_keyval_trigger_finalize (GtkShortcutTrigger *trigger)
{
}

static GtkShortcutTriggerMatch
gtk_keyval_trigger_trigger (GtkShortcutTrigger *trigger,
                            GdkEvent           *event,
                            gboolean            enable_mnemonics)
{
  GtkKeyvalTrigger *self = (GtkKeyvalTrigger *) trigger;
  guint keycode;
  GdkModifierType state;
  GdkModifierType mask;
  int group;
  GdkKeymap *keymap;
  guint keyval;
  int effective_group;
  int level;
  GdkModifierType consumed_modifiers;
  GdkModifierType shift_group_mask;
  gboolean group_mod_is_accel_mod = FALSE;
  const GdkModifierType xmods = GDK_MOD2_MASK|GDK_MOD3_MASK|GDK_MOD4_MASK|GDK_MOD5_MASK;
  const GdkModifierType vmods = GDK_SUPER_MASK|GDK_HYPER_MASK|GDK_META_MASK;
  GdkModifierType modifiers;

  if (gdk_event_get_event_type (event) != GDK_KEY_PRESS)
    return GTK_SHORTCUT_TRIGGER_MATCH_NONE;

  mask = gtk_accelerator_get_default_mod_mask ();

  keycode = gdk_key_event_get_keycode (event);
  state = gdk_event_get_modifier_state (event);
  group = gdk_key_event_get_group (event);
  keymap = gdk_display_get_keymap (gdk_event_get_display (event));

  /* We don't want Caps_Lock to affect keybinding lookups.
   */
  state &= ~GDK_LOCK_MASK;

  _gtk_translate_keyboard_accel_state (keymap,
                                       keycode, state, mask, group,
                                       &keyval,
                                       &effective_group, &level,
                                       &consumed_modifiers);

  /* if the group-toggling modifier is part of the default accel mod
   * mask, and it is active, disable it for matching
   */
  shift_group_mask = gdk_keymap_get_modifier_mask (keymap,
                                                   GDK_MODIFIER_INTENT_SHIFT_GROUP);
  if (mask & shift_group_mask)
    group_mod_is_accel_mod = TRUE;

  gdk_keymap_map_virtual_modifiers (keymap, &mask);
  gdk_keymap_add_virtual_modifiers (keymap, &state);


  modifiers = self->modifiers;
  if (gdk_keymap_map_virtual_modifiers (keymap, &modifiers) &&
      ((modifiers & ~consumed_modifiers & mask & ~vmods) == (state & ~consumed_modifiers & mask & ~vmods) ||
       (modifiers & ~consumed_modifiers & mask & ~xmods) == (state & ~consumed_modifiers & mask & ~xmods)))
    {
      /* modifier match */
      GdkKeymapKey *keys;
      int n_keys;
      int i;

      if (keyval == self->keyval && /* exact match */
          (!group_mod_is_accel_mod ||
           (state & shift_group_mask) == (self->modifiers & shift_group_mask)))
        return GTK_SHORTCUT_TRIGGER_MATCH_EXACT;

      gdk_keymap_get_entries_for_keyval (keymap,
                                         self->keyval,
                                         &keys, &n_keys);

      for (i = 0; i < n_keys; i++)
        {
          if (keys[i].keycode == keycode &&
              keys[i].level == level &&
              /* Only match for group if it's an accel mod */
              (!group_mod_is_accel_mod ||
               keys[i].group == effective_group))
            {
              /* partial match */
              g_free (keys);

              return GTK_SHORTCUT_TRIGGER_MATCH_PARTIAL;
            }
        }
 
      g_free (keys);
    }


  return GTK_SHORTCUT_TRIGGER_MATCH_NONE;
}

static guint
gtk_keyval_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkKeyvalTrigger *self = (GtkKeyvalTrigger *) trigger;

  return (self->modifiers << 24)
       | (self->modifiers >> 8)
       | (self->keyval << 16)
       | GTK_SHORTCUT_TRIGGER_KEYVAL;
}

static int
gtk_keyval_trigger_compare (GtkShortcutTrigger  *trigger1,
                            GtkShortcutTrigger  *trigger2)
{
  GtkKeyvalTrigger *self1 = (GtkKeyvalTrigger *) trigger1;
  GtkKeyvalTrigger *self2 = (GtkKeyvalTrigger *) trigger2;

  if (self1->modifiers != self2->modifiers)
    return self2->modifiers - self1->modifiers;

  return self1->keyval - self2->keyval;
}

static void
gtk_keyval_trigger_print (GtkShortcutTrigger *trigger,
                          GString            *string)
{
  GtkKeyvalTrigger *self = (GtkKeyvalTrigger *) trigger;
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
  GtkKeyvalTrigger *self = (GtkKeyvalTrigger *) trigger;

  gtk_accelerator_print_label (string, self->keyval, self->modifiers);

  return TRUE;
}

static const GtkShortcutTriggerClass GTK_KEYVAL_TRIGGER_CLASS = {
  GTK_SHORTCUT_TRIGGER_KEYVAL,
  sizeof (GtkKeyvalTrigger),
  "GtkKeyvalTrigger",
  gtk_keyval_trigger_finalize,
  gtk_keyval_trigger_trigger,
  gtk_keyval_trigger_hash,
  gtk_keyval_trigger_compare,
  gtk_keyval_trigger_print,
  gtk_keyval_trigger_print_label
};

/**
 * gtk_keyval_trigger_new:
 * @keyval: The keyval to trigger for
 * @modifiers: the modifiers that need to be present
 *
 * Creates a #GtkShortcutTrigger that will trigger whenever
 * the key with the given @keyval and @modifiers is pressed.
 *
 * Returns: A new #GtkShortcutTrigger
 */
GtkShortcutTrigger *
gtk_keyval_trigger_new (guint           keyval,
                        GdkModifierType modifiers)
{
  GtkKeyvalTrigger *self;

  self = (GtkKeyvalTrigger *) gtk_shortcut_trigger_new (&GTK_KEYVAL_TRIGGER_CLASS);

  /* We store keyvals as lower key */
  if (keyval == GDK_KEY_ISO_Left_Tab)
    self->keyval = GDK_KEY_Tab;
  else
    self->keyval = gdk_keyval_to_lower (keyval);
  self->modifiers = modifiers;

  return &self->trigger;
}

/**
 * gtk_keyval_trigger_get_modifiers:
 * @self: a keyval #GtkShortcutTrigger
 *
 * Gets the modifiers that must be present to succeed
 * triggering @self.
 *
 * Returns: the modifiers
 **/
GdkModifierType
gtk_keyval_trigger_get_modifiers (GtkShortcutTrigger *self)
{
  GtkKeyvalTrigger *trigger = (GtkKeyvalTrigger *) self;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER_TYPE (self, GTK_SHORTCUT_TRIGGER_KEYVAL), 0);

  return trigger->modifiers;
}

/**
 * gtk_keyval_trigger_get_keyval:
 * @self: a keyval #GtkShortcutTrigger
 *
 * Gets the keyval that must be pressed to succeed
 * triggering @self.
 *
 * Returns: the keyval
 **/
guint
gtk_keyval_trigger_get_keyval (GtkShortcutTrigger *self)
{
  GtkKeyvalTrigger *trigger = (GtkKeyvalTrigger *) self;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER_TYPE (self, GTK_SHORTCUT_TRIGGER_KEYVAL), 0);

  return trigger->keyval;
}

/*** GTK_MNEMONIC_TRIGGER ***/

typedef struct _GtkMnemonicTrigger GtkMnemonicTrigger;

struct _GtkMnemonicTrigger
{
  GtkShortcutTrigger trigger;

  guint keyval;
};

static void
gtk_mnemonic_trigger_finalize (GtkShortcutTrigger *trigger)
{
}

static GtkShortcutTriggerMatch
gtk_mnemonic_trigger_trigger (GtkShortcutTrigger *trigger,
                              GdkEvent           *event,
                              gboolean            enable_mnemonics)
{
  GtkMnemonicTrigger *self = (GtkMnemonicTrigger *) trigger;
  guint keyval;

  if (!enable_mnemonics)
    return GTK_SHORTCUT_TRIGGER_MATCH_NONE;

  if (gdk_event_get_event_type (event) != GDK_KEY_PRESS)
    return GTK_SHORTCUT_TRIGGER_MATCH_NONE;

  /* XXX: This needs to deal with groups */
  keyval = gdk_key_event_get_keyval (event);

  if (keyval == GDK_KEY_ISO_Left_Tab)
    keyval = GDK_KEY_Tab;
  else
    keyval = gdk_keyval_to_lower (keyval);

  if (keyval != self->keyval)
    return GTK_SHORTCUT_TRIGGER_MATCH_NONE;

  return GTK_SHORTCUT_TRIGGER_MATCH_EXACT;
}

static guint
gtk_mnemonic_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkMnemonicTrigger *self = (GtkMnemonicTrigger *) trigger;

  return (self->keyval << 8)
       | GTK_SHORTCUT_TRIGGER_MNEMONIC;
}

static int
gtk_mnemonic_trigger_compare (GtkShortcutTrigger  *trigger1,
                              GtkShortcutTrigger  *trigger2)
{
  GtkMnemonicTrigger *self1 = (GtkMnemonicTrigger *) trigger1;
  GtkMnemonicTrigger *self2 = (GtkMnemonicTrigger *) trigger2;

  return self1->keyval - self2->keyval;
}

static void
gtk_mnemonic_trigger_print (GtkShortcutTrigger *trigger,
                            GString            *string)
{
  GtkMnemonicTrigger *self = (GtkMnemonicTrigger *) trigger;
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
  GtkMnemonicTrigger *self = (GtkMnemonicTrigger *) trigger;
  const char *keyval_str;

  keyval_str = gdk_keyval_name (self->keyval);
  if (keyval_str == NULL)
    return FALSE;

  g_string_append (string, keyval_str);

  return TRUE;
}

static const GtkShortcutTriggerClass GTK_MNEMONIC_TRIGGER_CLASS = {
  GTK_SHORTCUT_TRIGGER_MNEMONIC,
  sizeof (GtkMnemonicTrigger),
  "GtkMnemonicTrigger",
  gtk_mnemonic_trigger_finalize,
  gtk_mnemonic_trigger_trigger,
  gtk_mnemonic_trigger_hash,
  gtk_mnemonic_trigger_compare,
  gtk_mnemonic_trigger_print,
  gtk_mnemonic_trigger_print_label
};

/**
 * gtk_mnemonic_trigger_new:
 * @keyval: The keyval to trigger for
 *
 * Creates a #GtkShortcutTrigger that will trigger whenever the key with
 * the given @keyval is pressed and mnemonics have been activated.
 *
 * Mnemonics are activated by calling code when a key event with the right
 * modifiers is detected.
 *
 * Returns: A new #GtkShortcutTrigger
 */
GtkShortcutTrigger *
gtk_mnemonic_trigger_new (guint keyval)
{
  GtkMnemonicTrigger *self;

  self = (GtkMnemonicTrigger *) gtk_shortcut_trigger_new (&GTK_MNEMONIC_TRIGGER_CLASS);

  /* We store keyvals as lower key */
  if (keyval == GDK_KEY_ISO_Left_Tab)
    self->keyval = GDK_KEY_Tab;
  else
    self->keyval = gdk_keyval_to_lower (keyval);

  return &self->trigger;
}

/**
 * gtk_mnemonic_trigger_get_keyval:
 * @trigger: a mnemonic #GtkShortcutTrigger
 *
 * Gets the keyval that must be pressed to succeed triggering @self.
 *
 * Returns: the keyval
 **/
guint
gtk_mnemonic_trigger_get_keyval (GtkShortcutTrigger *trigger)
{
  GtkMnemonicTrigger *self = (GtkMnemonicTrigger *) trigger;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER_TYPE (trigger, GTK_SHORTCUT_TRIGGER_MNEMONIC), 0);

  return self->keyval;
}

/*** GTK_ALTERNATIVE_TRIGGER ***/

typedef struct _GtkAlternativeTrigger GtkAlternativeTrigger;

struct _GtkAlternativeTrigger
{
  GtkShortcutTrigger trigger;

  GtkShortcutTrigger *first;
  GtkShortcutTrigger *second;
};

static void
gtk_alternative_trigger_finalize (GtkShortcutTrigger *trigger)
{
  GtkAlternativeTrigger *self = (GtkAlternativeTrigger *) trigger;

  gtk_shortcut_trigger_unref (self->first);
  gtk_shortcut_trigger_unref (self->second);
}

static GtkShortcutTriggerMatch
gtk_alternative_trigger_trigger (GtkShortcutTrigger *trigger,
                                 GdkEvent           *event,
                                 gboolean            enable_mnemonics)
{
  GtkAlternativeTrigger *self = (GtkAlternativeTrigger *) trigger;

  return MAX (gtk_shortcut_trigger_trigger (self->first, event, enable_mnemonics),
              gtk_shortcut_trigger_trigger (self->second, event, enable_mnemonics));
}

static guint
gtk_alternative_trigger_hash (GtkShortcutTrigger *trigger)
{
  GtkAlternativeTrigger *self = (GtkAlternativeTrigger *) trigger;
  guint result;

  result = gtk_shortcut_trigger_hash (self->first);
  result <<= 5;

  result |= gtk_shortcut_trigger_hash (self->second);
  result <<= 5;

  return result | GTK_SHORTCUT_TRIGGER_ALTERNATIVE;
}

static int
gtk_alternative_trigger_compare (GtkShortcutTrigger  *trigger1,
                                 GtkShortcutTrigger  *trigger2)
{
  GtkAlternativeTrigger *self1 = (GtkAlternativeTrigger *) trigger1;
  GtkAlternativeTrigger *self2 = (GtkAlternativeTrigger *) trigger2;
  int cmp;

  cmp = gtk_shortcut_trigger_compare (self1->first, self2->first);
  if (cmp != 0)
    return cmp;

  return gtk_shortcut_trigger_compare (self1->second, self2->second);
}

static void
gtk_alternative_trigger_print (GtkShortcutTrigger *trigger,
                               GString            *string)
{
  GtkAlternativeTrigger *self = (GtkAlternativeTrigger *) trigger;

  gtk_shortcut_trigger_print (self->first, string);
  g_string_append (string, ", ");
  gtk_shortcut_trigger_print (self->second, string);
}

static gboolean
gtk_alternative_trigger_print_label (GtkShortcutTrigger *trigger,
                                     GdkDisplay         *display,
                                     GString            *string)
{
  GtkAlternativeTrigger *self = (GtkAlternativeTrigger *) trigger;

  if (gtk_shortcut_trigger_print_label (self->first, display, string))
    {
      g_string_append (string, ", ");
      if (!gtk_shortcut_trigger_print_label (self->second, display, string))
        g_string_truncate (string, string->len - 2);
      return TRUE;
    }
  else
    {
      return gtk_shortcut_trigger_print_label (self->second, display, string);
    }
}

static const GtkShortcutTriggerClass GTK_ALTERNATIVE_TRIGGER_CLASS = {
  GTK_SHORTCUT_TRIGGER_ALTERNATIVE,
  sizeof (GtkAlternativeTrigger),
  "GtkAlternativeTrigger",
  gtk_alternative_trigger_finalize,
  gtk_alternative_trigger_trigger,
  gtk_alternative_trigger_hash,
  gtk_alternative_trigger_compare,
  gtk_alternative_trigger_print,
  gtk_alternative_trigger_print_label
};

/**
 * gtk_alternative_trigger_new:
 * @first: (transfer full): The first trigger that may trigger
 * @second: (transfer full): The second trigger that may trigger
 *
 * Creates a #GtkShortcutTrigger that will trigger whenever
 * either of the two given triggers gets triggered.
 *
 * Note that nesting is allowed, so if you want more than two
 * alternative, create a new alternative trigger for each option.
 *
 * Returns: a new #GtkShortcutTrigger
 */
GtkShortcutTrigger *
gtk_alternative_trigger_new (GtkShortcutTrigger *first,
                             GtkShortcutTrigger *second)
{
  GtkAlternativeTrigger *self;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (first), NULL);
  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER (second), NULL);

  self = (GtkAlternativeTrigger *) gtk_shortcut_trigger_new (&GTK_ALTERNATIVE_TRIGGER_CLASS);

  self->first = first;
  self->second = second;

  return &self->trigger;
}

/**
 * gtk_alternative_trigger_get_first:
 * @self: an alternative #GtkShortcutTrigger
 *
 * Gets the first of the two alternative triggers that may
 * trigger @self. gtk_alternative_trigger_get_second() will
 * return the other one.
 *
 * Returns: (transfer none): the first alternative trigger
 **/
GtkShortcutTrigger *
gtk_alternative_trigger_get_first (GtkShortcutTrigger *self)
{
  GtkAlternativeTrigger *trigger = (GtkAlternativeTrigger *) self;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER_TYPE (self, GTK_SHORTCUT_TRIGGER_ALTERNATIVE), 0);

  return trigger->first;
}

/**
 * gtk_alternative_trigger_get_second:
 * @self: an alternative #GtkShortcutTrigger
 *
 * Gets the second of the two alternative triggers that may
 * trigger @self. gtk_alternative_trigger_get_first() will
 * return the other one.
 *
 * Returns: (transfer none): the second alternative trigger
 **/
GtkShortcutTrigger *
gtk_alternative_trigger_get_second (GtkShortcutTrigger *self)
{
  GtkAlternativeTrigger *trigger = (GtkAlternativeTrigger *) self;

  g_return_val_if_fail (GTK_IS_SHORTCUT_TRIGGER_TYPE (self, GTK_SHORTCUT_TRIGGER_ALTERNATIVE), 0);

  return trigger->second;
}
