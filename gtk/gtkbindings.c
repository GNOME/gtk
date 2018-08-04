/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GtkBindingSet: Keybinding manager for GObjects.
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#include "config.h"
#include <string.h>
#include <stdarg.h>

#include "gtkbindingsprivate.h"
#include "gtkkeyhash.h"
#include "gtkstylecontext.h"
#include "gtkwidget.h"
#include "gtkintl.h"

/**
 * SECTION:gtkbindings
 * @Title: Bindings
 * @Short_description: Key bindings for individual widgets
 * @See_also: Keyboard Accelerators, Mnemonics, #GtkCssProvider
 *
 * #GtkBindingSet provides a mechanism for configuring GTK+ key bindings
 * through CSS files. This eases key binding adjustments for application
 * developers as well as users and provides GTK+ users or administrators
 * with high key  binding configurability which requires no application
 * or toolkit side changes.
 *
 * In order for bindings to work in a custom widget implementation, the
 * widget’s #GtkWidget:can-focus and #GtkWidget:has-focus properties
 * must both be true. For example, by calling gtk_widget_set_can_focus()
 * in the widget’s initialisation function; and by calling
 * gtk_widget_grab_focus() when the widget is clicked.
 *
 * # Installing a key binding
 *
 * A CSS file binding consists of a “binding-set” definition and a match
 * statement to apply the binding set to specific widget types. Details
 * on the matching mechanism are described under
 * [Selectors][gtkcssprovider-selectors]
 * in the #GtkCssProvider documentation. Inside the binding set
 * definition, key combinations are bound to one or more specific
 * signal emissions on the target widget. Key combinations are strings
 * consisting of an optional #GdkModifierType name and
 * [key names][gdk3-Keyboard-Handling]
 * such as those defined in `gdk/gdkkeysyms.h`
 * or returned from gdk_keyval_name(), they have to be parsable by
 * gtk_accelerator_parse(). Specifications of signal emissions consist
 * of a string identifying the signal name, and a list of signal specific
 * arguments in parenthesis.
 *
 * For example for binding Control and the left or right cursor keys
 * of a #GtkEntry widget to the #GtkEntry::move-cursor signal (so
 * movement occurs in 3-character steps), the following binding can be
 * used:
 *
 * |[ <!-- language="CSS" -->
 * @binding-set MoveCursor3
 * {
 *   bind "<Control>Right" { "move-cursor" (visual-positions, 3, 0) };
 *   bind "<Control>Left" { "move-cursor" (visual-positions, -3, 0) };
 * }
 *
 * entry
 * {
 *   -gtk-key-bindings: MoveCursor3;
 * }
 * ]|
 *
 * # Unbinding existing key bindings
 *
 * GTK+ already defines a number of useful bindings for the widgets
 * it provides. Because custom bindings set up in CSS files take
 * precedence over the default bindings shipped with GTK+, overriding
 * existing bindings as demonstrated in
 * [Installing a key binding][gtk-bindings-install]
 * works as expected. The same mechanism can not be used to “unbind”
 * existing bindings, however.
 *
 * |[ <!-- language="CSS" -->
 * @binding-set MoveCursor3
 * {
 *   bind "<Control>Right" {  };
 *   bind "<Control>Left" {  };
 * }
 *
 * entry
 * {
 *   -gtk-key-bindings: MoveCursor3;
 * }
 * ]|
 *
 * The above example will not have the desired effect of causing
 * “<Control>Right” and “<Control>Left” key presses to be ignored by GTK+.
 * Instead, it just causes any existing bindings from the bindings set
 * “MoveCursor3” to be deleted, so when “<Control>Right” or
 * “<Control>Left” are pressed, no binding for these keys is found in
 * binding set “MoveCursor3”. GTK+ will thus continue to search for
 * matching key bindings, and will eventually lookup and find the default
 * GTK+ bindings for entries which implement word movement. To keep GTK+
 * from activating its default bindings, the “unbind” keyword can be used
 * like this:
 *
 * |[ <!-- language="CSS" -->
 * @binding-set MoveCursor3
 * {
 *   unbind "<Control>Right";
 *   unbind "<Control>Left";
 * }
 *
 * entry
 * {
 *   -gtk-key-bindings: MoveCursor3;
 * }
 * ]|
 *
 * Now, GTK+ will find a match when looking up “<Control>Right” and
 * “<Control>Left” key presses before it resorts to its default bindings,
 * and the match instructs it to abort (“unbind”) the search, so the key
 * presses are not consumed by this widget. As usual, further processing
 * of the key presses, e.g. by an entry’s parent widget, is now possible.
 */

/* --- defines --- */
#define BINDING_MOD_MASK() (gtk_accelerator_get_default_mod_mask () | GDK_RELEASE_MASK)


/* --- structures --- */
typedef enum {
  GTK_BINDING_TOKEN_BIND,
  GTK_BINDING_TOKEN_UNBIND
} GtkBindingTokens;

typedef struct _GtkBindingEntry  GtkBindingEntry;
typedef struct _GtkBindingSignal GtkBindingSignal;

/**
 * GtkBindingSet:
 * @set_name: unique name of this binding set
 * @priority: unused
 * @entries: the key binding entries in this binding set
 * @current: implementation detail
 *
 * A binding set maintains a list of activatable key bindings.
 * A single binding set can match multiple types of widgets.
 * Similar to style contexts, can be matched by any information contained
 * in a widgets #GtkWidgetPath. When a binding within a set is matched upon
 * activation, an action signal is emitted on the target widget to carry out
 * the actual activation.
 */
struct _GtkBindingSet
{
  gchar           *set_name;
  gint             priority;
  GtkBindingEntry *entries;
  GtkBindingEntry *current;
};

/**
 * GtkBindingEntry:
 * @keyval: key value to match
 * @modifiers: key modifiers to match
 * @binding_set: binding set this entry belongs to
 * @destroyed: implementation detail
 * @in_emission: implementation detail
 * @marks_unbound: implementation detail
 * @set_next: linked list of entries maintained by binding set
 * @hash_next: implementation detail
 * @signals: action signals of this entry
 *
 * Each key binding element of a binding sets binding list is
 * represented by a GtkBindingEntry.
 */
struct _GtkBindingEntry
{
  /* key portion */
  guint             keyval;
  GdkModifierType   modifiers;

  GtkBindingSet    *binding_set;
  guint             destroyed     : 1;
  guint             in_emission   : 1;
  guint             marks_unbound : 1;
  GtkBindingEntry  *set_next;
  GtkBindingEntry  *hash_next;
  GtkBindingSignal *signals;
};

/**
 * GtkBindingSignal:
 * @next: implementation detail
 * @signal_name: the action signal to be emitted
 * @n_args: number of arguments specified for the signal
 * @args: (array length=n_args): the arguments specified for the signal
 *
 * A GtkBindingSignal stores the necessary information to
 * activate a widget in response to a key press via a signal
 * emission.
 */
struct _GtkBindingSignal
{
  GtkBindingSignal *next;
  gchar            *signal_name;
  GVariant         *args;
};

/* --- variables --- */
static GHashTable       *binding_entry_hash_table = NULL;
static GSList           *binding_key_hashes = NULL;
static GSList           *binding_set_list = NULL;
static const gchar       key_class_binding_set[] = "gtk-class-binding-set";
static GQuark            key_id_class_binding_set = 0;


/* --- functions --- */

static GtkBindingSignal*
binding_signal_new (const gchar *signal_name,
                    GVariant    *args)
{
  GtkBindingSignal *signal;

  signal = g_slice_new0 (GtkBindingSignal);
  signal->next = NULL;
  signal->signal_name = (gchar *)g_intern_string (signal_name);
  signal->args = g_variant_ref_sink (args);

  return signal;
}

static void
binding_signal_free (GtkBindingSignal *sig)
{
  g_variant_unref (sig->args);
  g_slice_free (GtkBindingSignal, sig);
}

static guint
binding_entry_hash (gconstpointer  key)
{
  register const GtkBindingEntry *e = key;
  register guint h;

  h = e->keyval;
  h ^= e->modifiers;

  return h;
}

static gint
binding_entries_compare (gconstpointer  a,
                         gconstpointer  b)
{
  register const GtkBindingEntry *ea = a;
  register const GtkBindingEntry *eb = b;

  return (ea->keyval == eb->keyval && ea->modifiers == eb->modifiers);
}

static void
binding_key_hash_insert_entry (GtkKeyHash      *key_hash,
                               GtkBindingEntry *entry)
{
  guint keyval = entry->keyval;

  /* We store lowercased accelerators. To deal with this, if <Shift>
   * was specified, uppercase.
   */
  if (entry->modifiers & GDK_SHIFT_MASK)
    {
      if (keyval == GDK_KEY_Tab)
        keyval = GDK_KEY_ISO_Left_Tab;
      else
        keyval = gdk_keyval_to_upper (keyval);
    }

  _gtk_key_hash_add_entry (key_hash, keyval, entry->modifiers & ~GDK_RELEASE_MASK, entry);
}

static void
binding_key_hash_destroy (gpointer data)
{
  GtkKeyHash *key_hash = data;

  binding_key_hashes = g_slist_remove (binding_key_hashes, key_hash);
  _gtk_key_hash_free (key_hash);
}

static void
insert_entries_into_key_hash (gpointer key,
                              gpointer value,
                              gpointer data)
{
  GtkKeyHash *key_hash = data;
  GtkBindingEntry *entry = value;

  for (; entry; entry = entry->hash_next)
    binding_key_hash_insert_entry (key_hash, entry);
}

static GtkKeyHash *
binding_key_hash_for_keymap (GdkKeymap *keymap)
{
  static GQuark key_hash_quark = 0;
  GtkKeyHash *key_hash;

  if (!key_hash_quark)
    key_hash_quark = g_quark_from_static_string ("gtk-binding-key-hash");

  key_hash = g_object_get_qdata (G_OBJECT (keymap), key_hash_quark);

  if (!key_hash)
    {
      key_hash = _gtk_key_hash_new (keymap, NULL);
      g_object_set_qdata_full (G_OBJECT (keymap), key_hash_quark, key_hash, binding_key_hash_destroy);

      if (binding_entry_hash_table)
        g_hash_table_foreach (binding_entry_hash_table,
                              insert_entries_into_key_hash,
                              key_hash);

      binding_key_hashes = g_slist_prepend (binding_key_hashes, key_hash);
    }

  return key_hash;
}


static GtkBindingEntry*
binding_entry_new (GtkBindingSet  *binding_set,
                   guint           keyval,
                   GdkModifierType modifiers)
{
  GSList *tmp_list;
  GtkBindingEntry *entry;

  if (!binding_entry_hash_table)
    binding_entry_hash_table = g_hash_table_new (binding_entry_hash, binding_entries_compare);

  entry = g_new (GtkBindingEntry, 1);
  entry->keyval = keyval;
  entry->modifiers = modifiers;
  entry->binding_set = binding_set,
  entry->destroyed = FALSE;
  entry->in_emission = FALSE;
  entry->marks_unbound = FALSE;
  entry->signals = NULL;

  entry->set_next = binding_set->entries;
  binding_set->entries = entry;

  entry->hash_next = g_hash_table_lookup (binding_entry_hash_table, entry);
  if (entry->hash_next)
    g_hash_table_remove (binding_entry_hash_table, entry->hash_next);
  g_hash_table_insert (binding_entry_hash_table, entry, entry);

  for (tmp_list = binding_key_hashes; tmp_list; tmp_list = tmp_list->next)
    {
      GtkKeyHash *key_hash = tmp_list->data;
      binding_key_hash_insert_entry (key_hash, entry);
    }

  return entry;
}

static void
binding_entry_free (GtkBindingEntry *entry)
{
  GtkBindingSignal *sig;

  g_assert (entry->set_next == NULL &&
            entry->hash_next == NULL &&
            entry->in_emission == FALSE &&
            entry->destroyed == TRUE);

  entry->destroyed = FALSE;

  sig = entry->signals;
  while (sig)
    {
      GtkBindingSignal *prev;

      prev = sig;
      sig = prev->next;
      binding_signal_free (prev);
    }
  g_free (entry);
}

static void
binding_entry_destroy (GtkBindingEntry *entry)
{
  GtkBindingEntry *o_entry;
  register GtkBindingEntry *tmp;
  GtkBindingEntry *begin;
  register GtkBindingEntry *last;
  GSList *tmp_list;

  /* unlink from binding set
   */
  last = NULL;
  tmp = entry->binding_set->entries;
  while (tmp)
    {
      if (tmp == entry)
        {
          if (last)
            last->set_next = entry->set_next;
          else
            entry->binding_set->entries = entry->set_next;
          break;
        }
      last = tmp;
      tmp = last->set_next;
    }
  entry->set_next = NULL;

  o_entry = g_hash_table_lookup (binding_entry_hash_table, entry);
  begin = o_entry;
  last = NULL;
  tmp = begin;
  while (tmp)
    {
      if (tmp == entry)
        {
          if (last)
            last->hash_next = entry->hash_next;
          else
            begin = entry->hash_next;
          break;
        }
      last = tmp;
      tmp = last->hash_next;
    }
  entry->hash_next = NULL;

  if (!begin)
    g_hash_table_remove (binding_entry_hash_table, entry);
  else if (begin != o_entry)
    {
      g_hash_table_remove (binding_entry_hash_table, entry);
      g_hash_table_insert (binding_entry_hash_table, begin, begin);
    }

  for (tmp_list = binding_key_hashes; tmp_list; tmp_list = tmp_list->next)
    {
      GtkKeyHash *key_hash = tmp_list->data;
      _gtk_key_hash_remove_entry (key_hash, entry);
    }

  entry->destroyed = TRUE;

  if (!entry->in_emission)
    binding_entry_free (entry);
}

static GtkBindingEntry*
binding_ht_lookup_entry (GtkBindingSet  *set,
                         guint           keyval,
                         GdkModifierType modifiers)
{
  GtkBindingEntry lookup_entry = { 0 };
  GtkBindingEntry *entry;

  if (!binding_entry_hash_table)
    return NULL;

  lookup_entry.keyval = keyval;
  lookup_entry.modifiers = modifiers;

  entry = g_hash_table_lookup (binding_entry_hash_table, &lookup_entry);
  for (; entry; entry = entry->hash_next)
    if (entry->binding_set == set)
      return entry;

  return NULL;
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
        case G_VARIANT_CLASS_DOUBLE:
          g_value_init (&tmp_value, G_TYPE_DOUBLE);
          g_value_set_double (&tmp_value, g_variant_get_double (tmp_variant));
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
gtk_binding_entry_activate (GtkBindingEntry *entry,
                            GObject         *object)
{
  GtkBindingSignal *sig;
  gboolean old_emission;
  gboolean handled = FALSE;
  gint i;

  old_emission = entry->in_emission;
  entry->in_emission = TRUE;

  g_object_ref (object);

  for (sig = entry->signals; sig; sig = sig->next)
    {
      GSignalQuery query;
      guint signal_id;
      GValue *params = NULL;
      GValue return_val = G_VALUE_INIT;
      gchar *accelerator = NULL;
      GVariantIter args_iter;
      gsize n_args;

      signal_id = g_signal_lookup (sig->signal_name, G_OBJECT_TYPE (object));
      if (!signal_id)
        {
          accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
          g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
                     "could not find signal \"%s\" in the '%s' class ancestry",
                     entry->binding_set->set_name,
                     accelerator,
                     sig->signal_name,
                     g_type_name (G_OBJECT_TYPE (object)));
          g_free (accelerator);
          continue;
        }

      g_signal_query (signal_id, &query);
      if (sig->args)
        n_args = g_variant_iter_init (&args_iter, sig->args);
      else
        n_args = 0;
      if (query.n_params != n_args ||
          (query.return_type != G_TYPE_NONE && query.return_type != G_TYPE_BOOLEAN) ||
          !binding_compose_params (object, &args_iter, &query, &params))
        {
          accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
          g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
                     "signature mismatch for signal \"%s\" in the '%s' class ancestry",
                     entry->binding_set->set_name,
                     accelerator,
                     sig->signal_name,
                     g_type_name (G_OBJECT_TYPE (object)));
        }
      else if (!(query.signal_flags & G_SIGNAL_ACTION))
        {
          accelerator = gtk_accelerator_name (entry->keyval, entry->modifiers);
          g_warning ("gtk_binding_entry_activate(): binding \"%s::%s\": "
                     "signal \"%s\" in the '%s' class ancestry cannot be used for action emissions",
                     entry->binding_set->set_name,
                     accelerator,
                     sig->signal_name,
                     g_type_name (G_OBJECT_TYPE (object)));
        }
      g_free (accelerator);
      if (accelerator)
        continue;

      if (query.return_type == G_TYPE_BOOLEAN)
        g_value_init (&return_val, G_TYPE_BOOLEAN);

      g_signal_emitv (params, signal_id, 0, &return_val);

      if (query.return_type == G_TYPE_BOOLEAN)
        {
          if (g_value_get_boolean (&return_val))
            handled = TRUE;
          g_value_unset (&return_val);
        }
      else
        handled = TRUE;

      if (params != NULL)
        {
          for (i = 0; i < query.n_params + 1; i++)
            g_value_unset (&params[i]);

          g_free (params);
        }

      if (entry->destroyed)
        break;
    }

  g_object_unref (object);

  entry->in_emission = old_emission;
  if (entry->destroyed && !entry->in_emission)
    binding_entry_free (entry);

  return handled;
}

/**
 * gtk_binding_set_new: (skip)
 * @set_name: unique name of this binding set
 *
 * GTK+ maintains a global list of binding sets. Each binding set has
 * a unique name which needs to be specified upon creation.
 *
 * Returns: (transfer none): new binding set
 */
GtkBindingSet*
gtk_binding_set_new (const gchar *set_name)
{
  GtkBindingSet *binding_set;

  g_return_val_if_fail (set_name != NULL, NULL);

  binding_set = g_new (GtkBindingSet, 1);
  binding_set->set_name = (gchar *) g_intern_string (set_name);
  binding_set->entries = NULL;
  binding_set->current = NULL;

  binding_set_list = g_slist_prepend (binding_set_list, binding_set);

  return binding_set;
}

/**
 * gtk_binding_set_by_class: (skip)
 * @object_class: a valid #GObject class
 *
 * This function returns the binding set named after the type name of
 * the passed in class structure. New binding sets are created on
 * demand by this function.
 *
 * Returns: (transfer none): the binding set corresponding to
 *     @object_class
 */
GtkBindingSet*
gtk_binding_set_by_class (gpointer object_class)
{
  GObjectClass *class = object_class;
  GtkBindingSet* binding_set;

  g_return_val_if_fail (G_IS_OBJECT_CLASS (class), NULL);

  if (!key_id_class_binding_set)
    key_id_class_binding_set = g_quark_from_static_string (key_class_binding_set);

  binding_set = g_dataset_id_get_data (class, key_id_class_binding_set);

  if (binding_set)
    return binding_set;

  binding_set = gtk_binding_set_new (g_type_name (G_OBJECT_CLASS_TYPE (class)));
  g_dataset_id_set_data (class, key_id_class_binding_set, binding_set);

  return binding_set;
}

static GtkBindingSet*
gtk_binding_set_find_interned (const gchar *set_name)
{
  GSList *slist;

  for (slist = binding_set_list; slist; slist = slist->next)
    {
      GtkBindingSet *binding_set;

      binding_set = slist->data;
      if (binding_set->set_name == set_name)
        return binding_set;
    }

  return NULL;
}

/**
 * gtk_binding_set_find:
 * @set_name: unique binding set name
 *
 * Find a binding set by its globally unique name.
 *
 * The @set_name can either be a name used for gtk_binding_set_new()
 * or the type name of a class used in gtk_binding_set_by_class().
 *
 * Returns: (nullable) (transfer none): %NULL or the specified binding set
 */
GtkBindingSet*
gtk_binding_set_find (const gchar *set_name)
{
  g_return_val_if_fail (set_name != NULL, NULL);

  return gtk_binding_set_find_interned (g_intern_string (set_name));
}

/**
 * gtk_binding_set_activate:
 * @binding_set: a #GtkBindingSet set to activate
 * @keyval:      key value of the binding
 * @modifiers:   key modifier of the binding
 * @object:      object to activate when binding found
 *
 * Find a key binding matching @keyval and @modifiers within
 * @binding_set and activate the binding on @object.
 *
 * Returns: %TRUE if a binding was found and activated
 */
gboolean
gtk_binding_set_activate (GtkBindingSet  *binding_set,
                          guint           keyval,
                          GdkModifierType modifiers,
                          GObject        *object)
{
  GtkBindingEntry *entry;

  g_return_val_if_fail (binding_set != NULL, FALSE);
  g_return_val_if_fail (G_IS_OBJECT (object), FALSE);

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    return gtk_binding_entry_activate (entry, object);

  return FALSE;
}

static void
gtk_binding_entry_clear_internal (GtkBindingSet  *binding_set,
                                  guint           keyval,
                                  GdkModifierType modifiers)
{
  GtkBindingEntry *entry;

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);

  entry = binding_entry_new (binding_set, keyval, modifiers);
}

/**
 * gtk_binding_entry_skip:
 * @binding_set: a #GtkBindingSet to skip an entry of
 * @keyval:      key value of binding to skip
 * @modifiers:   key modifier of binding to skip
 *
 * Install a binding on @binding_set which causes key lookups
 * to be aborted, to prevent bindings from lower priority sets
 * to be activated.
 */
void
gtk_binding_entry_skip (GtkBindingSet  *binding_set,
                        guint           keyval,
                        GdkModifierType modifiers)
{
  GtkBindingEntry *entry;

  g_return_if_fail (binding_set != NULL);

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);

  entry = binding_entry_new (binding_set, keyval, modifiers);
  entry->marks_unbound = TRUE;
}

/**
 * gtk_binding_entry_remove:
 * @binding_set: a #GtkBindingSet to remove an entry of
 * @keyval:      key value of binding to remove
 * @modifiers:   key modifier of binding to remove
 *
 * Remove a binding previously installed via
 * gtk_binding_entry_add_signal() on @binding_set.
 */
void
gtk_binding_entry_remove (GtkBindingSet  *binding_set,
                          guint           keyval,
                          GdkModifierType modifiers)
{
  GtkBindingEntry *entry;

  g_return_if_fail (binding_set != NULL);

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (entry)
    binding_entry_destroy (entry);
}

/**
 * gtk_binding_entry_add_signal_variant:
 * @binding_set:  a #GtkBindingSet to add a signal to
 * @keyval:       key value
 * @modifiers:    key modifier
 * @signal_name:  signal name to be bound
 * @binding_args: a #GVariant containing a struct with the arguments to pass.
 *
 * Override or install a new key binding for @keyval with @modifiers on
 * @binding_set.
 */
void
gtk_binding_entry_add_signal_variant (GtkBindingSet  *binding_set,
                                      guint           keyval,
                                      GdkModifierType modifiers,
                                      const gchar    *signal_name,
                                      GVariant       *binding_args)
{
  GtkBindingEntry *entry;
  GtkBindingSignal *signal, **signal_p;

  keyval = gdk_keyval_to_lower (keyval);
  modifiers = modifiers & BINDING_MOD_MASK ();

  signal = binding_signal_new (signal_name, binding_args);

  entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
  if (!entry)
    {
      gtk_binding_entry_clear_internal (binding_set, keyval, modifiers);
      entry = binding_ht_lookup_entry (binding_set, keyval, modifiers);
    }
  signal_p = &entry->signals;
  while (*signal_p)
    signal_p = &(*signal_p)->next;
  *signal_p = signal;
}

/**
 * gtk_binding_entry_add_signal:
 * @binding_set: a #GtkBindingSet to install an entry for
 * @keyval:      key value of binding to install
 * @modifiers:   key modifier of binding to install
 * @signal_name: signal to execute upon activation
 * @n_args:      number of arguments to @signal_name
 * @...:         arguments to @signal_name
 *
 * Override or install a new key binding for @keyval with @modifiers on
 * @binding_set. When the binding is activated, @signal_name will be
 * emitted on the target widget, with @n_args @Varargs used as
 * arguments.
 *
 * Each argument to the signal must be passed as a pair of varargs: the
 * #GType of the argument, followed by the argument value (which must
 * be of the given type). There must be @n_args pairs in total.
 *
 * ## Adding a Key Binding
 *
 * |[<!-- language="C" -->
 * GtkBindingSet *binding_set;
 * GdkModifierType modmask = GDK_CONTROL_MASK;
 * int count = 1;
 * gtk_binding_entry_add_signal (binding_set,
 *                               GDK_KEY_space,
 *                               modmask,
 *                               "move-cursor", 2,
 *                               GTK_TYPE_MOVEMENT_STEP, GTK_MOVEMENT_PAGES,
 *                               G_TYPE_INT, count,
 *                               G_TYPE_BOOLEAN, FALSE);
 * ]|
 */
void
gtk_binding_entry_add_signal (GtkBindingSet  *binding_set,
                              guint           keyval,
                              GdkModifierType modifiers,
                              const gchar    *signal_name,
                              guint           n_args,
                              ...)
{
  GVariantBuilder builder;
  va_list args;
  GType arg_type;
  guint i;

  g_return_if_fail (binding_set != NULL);
  g_return_if_fail (signal_name != NULL);

  va_start (args, n_args);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  for (i = 0; i < n_args; i++)
    {
      arg_type = va_arg (args, GType);
      switch (G_TYPE_FUNDAMENTAL (arg_type))
        {
        case G_TYPE_CHAR:
        case G_TYPE_UCHAR:
        case G_TYPE_INT:
        case G_TYPE_UINT:
        case G_TYPE_BOOLEAN:
        case G_TYPE_ENUM:
        case G_TYPE_FLAGS:
          g_variant_builder_add (&builder, "x", (gint64) va_arg (args, gint));
          break;
        case G_TYPE_LONG:
        case G_TYPE_ULONG:
          g_variant_builder_add (&builder, "x", (gint64) va_arg (args, glong));
          break;
        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
          g_variant_builder_add (&builder, "d", (gint64) va_arg (args, gdouble));
          break;
        case G_TYPE_STRING:
          {
            char *s = va_arg (args, gchar*);
            if (s != NULL)
              {
                g_variant_builder_add (&builder, "s", s);
              }
            else
              {
                g_warning ("gtk_binding_entry_add_signal(): type '%s' arg[%u] is 'NULL'",
                           g_type_name (arg_type),
                           i);
                i += n_args + 1;
              }
            }
          break;
        default:
          g_warning ("gtk_binding_entry_add_signal(): unsupported type '%s' for arg[%u]",
                     g_type_name (arg_type), i);
          i += n_args + 1;
          break;
        }
    }
  va_end (args);

  if (i == n_args || i == 0)
    gtk_binding_entry_add_signal_variant (binding_set, keyval, modifiers, signal_name, g_variant_builder_end (&builder));
  else
    g_variant_builder_clear (&builder);
}

static guint
gtk_binding_parse_signal (GScanner       *scanner,
                          GtkBindingSet  *binding_set,
                          guint           keyval,
                          GdkModifierType modifiers)
{
  gchar *signal;
  guint expected_token = 0;
  gboolean done;
  gboolean negate;
  gboolean need_arg;
  gboolean seen_comma;
  GVariantBuilder builder;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_STRING)
    return G_TOKEN_STRING;

  g_scanner_peek_next_token (scanner);

  if (scanner->next_token != '(')
    {
      g_scanner_get_next_token (scanner);
      return '(';
    }

  signal = g_strdup (scanner->value.v_string);
  g_scanner_get_next_token (scanner);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  negate = FALSE;
  done = FALSE;
  need_arg = TRUE;
  seen_comma = FALSE;
  scanner->config->scan_symbols = FALSE;

  do
    {
      if (need_arg)
        expected_token = G_TOKEN_INT;
      else
        expected_token = ')';

      g_scanner_get_next_token (scanner);

      switch ((guint) scanner->token)
        {
        case G_TOKEN_FLOAT:
          if (need_arg)
            {
              if (negate)
                g_variant_builder_add (&builder, "d", (double) scanner->value.v_float);
              else
                g_variant_builder_add (&builder, "d", (double) - scanner->value.v_float);

              need_arg = FALSE;
              negate = FALSE;
            }
          else
            done = TRUE;

          break;
        case G_TOKEN_INT:
          if (need_arg)
            {
              if (negate)
                g_variant_builder_add (&builder, "x", (gint64) - scanner->value.v_int);
              else
                g_variant_builder_add (&builder, "x", (gint64) scanner->value.v_int);

              need_arg = FALSE;
              negate = FALSE;
            }
          else
            done = TRUE;
          break;
        case G_TOKEN_STRING:
          if (need_arg && !negate)
            {
              need_arg = FALSE;
              g_variant_builder_add (&builder, "s", scanner->value.v_string);
            }
          else
            done = TRUE;

          break;
        case G_TOKEN_IDENTIFIER:
          if (need_arg && !negate)
            {
              need_arg = FALSE;
              g_variant_builder_add (&builder, "s", scanner->value.v_string);
            }
          else
            done = TRUE;

          break;
        case '-':
          if (!need_arg)
            done = TRUE;
          else if (negate)
            {
              expected_token = G_TOKEN_INT;
              done = TRUE;
            }
          else
            negate = TRUE;

          break;
        case ',':
          seen_comma = TRUE;
          if (need_arg)
            done = TRUE;
          else
            need_arg = TRUE;

          break;
        case ')':
          if (!(need_arg && seen_comma) && !negate)
            {
              gtk_binding_entry_add_signal_variant (binding_set,
                                                    keyval,
                                                    modifiers,
                                                    signal,
                                                    g_variant_builder_end (&builder));
              expected_token = G_TOKEN_NONE;
            }

          done = TRUE;
          break;
        default:
          done = TRUE;
          break;
        }
    }
  while (!done);

  scanner->config->scan_symbols = TRUE;

  g_variant_builder_clear (&builder);

  g_free (signal);

  return expected_token;
}

static inline guint
gtk_binding_parse_bind (GScanner       *scanner,
                        GtkBindingSet  *binding_set)
{
  guint keyval = 0;
  GdkModifierType modifiers = 0;
  gboolean unbind = FALSE;

  g_return_val_if_fail (scanner != NULL, G_TOKEN_ERROR);

  g_scanner_get_next_token (scanner);

  if (scanner->token != G_TOKEN_SYMBOL)
    return G_TOKEN_SYMBOL;

  if (scanner->value.v_symbol != GUINT_TO_POINTER (GTK_BINDING_TOKEN_BIND) &&
      scanner->value.v_symbol != GUINT_TO_POINTER (GTK_BINDING_TOKEN_UNBIND))
    return G_TOKEN_SYMBOL;

  unbind = (scanner->value.v_symbol == GUINT_TO_POINTER (GTK_BINDING_TOKEN_UNBIND));
  g_scanner_get_next_token (scanner);

  if (scanner->token != (guint) G_TOKEN_STRING)
    return G_TOKEN_STRING;

  gtk_accelerator_parse (scanner->value.v_string, &keyval, &modifiers);
  modifiers &= BINDING_MOD_MASK ();

  if (keyval == 0)
    return G_TOKEN_STRING;

  if (unbind)
    {
      gtk_binding_entry_skip (binding_set, keyval, modifiers);
      return G_TOKEN_NONE;
    }

  g_scanner_get_next_token (scanner);

  if (scanner->token != '{')
    return '{';

  gtk_binding_entry_clear_internal (binding_set, keyval, modifiers);
  g_scanner_peek_next_token (scanner);

  while (scanner->next_token != '}')
    {
      guint expected_token;

      if (scanner->next_token == G_TOKEN_STRING)
        {
          expected_token = gtk_binding_parse_signal (scanner,
                                                     binding_set,
                                                     keyval,
                                                     modifiers);
          if (expected_token != G_TOKEN_NONE)
            return expected_token;
        }
      else
        {
          g_scanner_get_next_token (scanner);
          return '}';
        }

      g_scanner_peek_next_token (scanner);
    }

  g_scanner_get_next_token (scanner);

  return G_TOKEN_NONE;
}

static GScanner *
create_signal_scanner (void)
{
  GScanner *scanner;

  scanner = g_scanner_new (NULL);
  scanner->config->cset_identifier_nth = (char *) G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "-_";

  g_scanner_scope_add_symbol (scanner, 0, "bind", GUINT_TO_POINTER (GTK_BINDING_TOKEN_BIND));
  g_scanner_scope_add_symbol (scanner, 0, "unbind", GUINT_TO_POINTER (GTK_BINDING_TOKEN_UNBIND));

  g_scanner_set_scope (scanner, 0);

  return scanner;
}

/**
 * gtk_binding_entry_add_signal_from_string:
 * @binding_set: a #GtkBindingSet
 * @signal_desc: a signal description
 *
 * Parses a signal description from @signal_desc and incorporates
 * it into @binding_set.
 *
 * Signal descriptions may either bind a key combination to
 * one or more signals:
 * |[
 *   bind "key" {
 *     "signalname" (param, ...)
 *     ...
 *   }
 * ]|
 *
 * Or they may also unbind a key combination:
 * |[
 *   unbind "key"
 * ]|
 *
 * Key combinations must be in a format that can be parsed by
 * gtk_accelerator_parse().
 *
 * Returns: %G_TOKEN_NONE if the signal was successfully parsed and added,
 *     the expected token otherwise
 */
GTokenType
gtk_binding_entry_add_signal_from_string (GtkBindingSet *binding_set,
                                          const gchar   *signal_desc)
{
  static GScanner *scanner = NULL;
  GTokenType ret;

  g_return_val_if_fail (binding_set != NULL, G_TOKEN_NONE);
  g_return_val_if_fail (signal_desc != NULL, G_TOKEN_NONE);

  if (G_UNLIKELY (!scanner))
    scanner = create_signal_scanner ();

  g_scanner_input_text (scanner, signal_desc,
                        (guint) strlen (signal_desc));

  ret = gtk_binding_parse_bind (scanner, binding_set);

  /* Reset for next use */
  g_scanner_set_scope (scanner, 0);

  return ret;
}

static gint
find_entry_with_binding (GtkBindingEntry *entry,
                         GtkBindingSet   *binding_set)
{
  return (entry->binding_set == binding_set) ? 0 : 1;
}

static gboolean
binding_activate (GtkBindingSet *binding_set,
                  GSList        *entries,
                  GObject       *object,
                  gboolean       is_release,
                  gboolean      *unbound)
{
  GtkBindingEntry *entry;
  GSList *elem;

  elem = g_slist_find_custom (entries, binding_set,
                              (GCompareFunc) find_entry_with_binding);

  if (!elem)
    return FALSE;

  entry = elem->data;

  if (is_release != ((entry->modifiers & GDK_RELEASE_MASK) != 0))
    return FALSE;

  if (entry->marks_unbound)
    {
      *unbound = TRUE;
      return FALSE;
    }

  if (gtk_binding_entry_activate (entry, object))
    return TRUE;

  return FALSE;
}

static gboolean
gtk_bindings_activate_list (GObject  *object,
                            GSList   *entries,
                            gboolean  is_release)
{
  GtkStyleContext *context;
  GtkBindingSet *binding_set;
  gboolean handled = FALSE;
  gboolean unbound = FALSE;
  GPtrArray *array;

  if (!entries)
    return FALSE;

  context = gtk_widget_get_style_context (GTK_WIDGET (object));

  gtk_style_context_get (context,
                         "-gtk-key-bindings", &array,
                         NULL);
  if (array)
    {
      gint i;

      for (i = 0; i < array->len; i++)
        {
          binding_set = g_ptr_array_index (array, i);
          handled = binding_activate (binding_set, entries,
                                      object, is_release,
                                      &unbound);
          if (handled || unbound)
            break;
        }

      g_ptr_array_unref (array);

      if (unbound)
        return FALSE;
    }

  if (!handled)
    {
      GType class_type;

      class_type = G_TYPE_FROM_INSTANCE (object);

      while (class_type && !handled)
        {
          binding_set = gtk_binding_set_find_interned (g_type_name (class_type));
          class_type = g_type_parent (class_type);

          if (!binding_set)
            continue;

          handled = binding_activate (binding_set, entries,
                                      object, is_release,
                                      &unbound);
          if (unbound)
            break;
        }

      if (unbound)
        return FALSE;
    }

  return handled;
}

/**
 * gtk_bindings_activate:
 * @object: object to activate when binding found
 * @keyval: key value of the binding
 * @modifiers: key modifier of the binding
 *
 * Find a key binding matching @keyval and @modifiers and activate the
 * binding on @object.
 *
 * Returns: %TRUE if a binding was found and activated
 */
gboolean
gtk_bindings_activate (GObject         *object,
                       guint            keyval,
                       GdkModifierType  modifiers)
{
  GSList *entries = NULL;
  GdkDisplay *display;
  GtkKeyHash *key_hash;
  gboolean handled = FALSE;
  gboolean is_release;

  if (!GTK_IS_WIDGET (object))
    return FALSE;

  is_release = (modifiers & GDK_RELEASE_MASK) != 0;
  modifiers = modifiers & BINDING_MOD_MASK () & ~GDK_RELEASE_MASK;

  display = gtk_widget_get_display (GTK_WIDGET (object));
  key_hash = binding_key_hash_for_keymap (gdk_display_get_keymap (display));

  entries = _gtk_key_hash_lookup_keyval (key_hash, keyval, modifiers);

  handled = gtk_bindings_activate_list (object, entries, is_release);

  g_slist_free (entries);

  return handled;
}

/**
 * gtk_bindings_activate_event:
 * @object: a #GObject (generally must be a widget)
 * @event: a #GdkEventKey
 *
 * Looks up key bindings for @object to find one matching
 * @event, and if one was found, activate it.
 *
 * Returns: %TRUE if a matching key binding was found
 */
gboolean
gtk_bindings_activate_event (GObject     *object,
                             GdkEventKey *event)
{
  GSList *entries = NULL;
  GdkDisplay *display;
  GtkKeyHash *key_hash;
  gboolean handled = FALSE;
  guint16 keycode;
  GdkModifierType state;
  guint group;

  if (!GTK_IS_WIDGET (object))
    return FALSE;

  display = gtk_widget_get_display (GTK_WIDGET (object));
  key_hash = binding_key_hash_for_keymap (gdk_display_get_keymap (display));

  gdk_event_get_keycode ((GdkEvent *)event, &keycode);
  gdk_event_get_state ((GdkEvent *)event, &state);
  gdk_event_get_key_group ((GdkEvent *)event, &group);

  entries = _gtk_key_hash_lookup (key_hash,
                                  keycode,
                                  state,
                                  BINDING_MOD_MASK () & ~GDK_RELEASE_MASK,
                                  group);

  handled = gtk_bindings_activate_list (object, entries,
                                        gdk_event_get_event_type ((GdkEvent *) event) == GDK_KEY_RELEASE);

  g_slist_free (entries);

  return handled;
}
