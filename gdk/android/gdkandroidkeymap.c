/*
 * Copyright (c) 2024 Florian "sp1rit" <sp1rit@disroot.org>
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <gdk/gdk.h>

#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"

#include <android/keycodes.h>

#include "gdkandroidkeysyms-private.h"

#include "gdkandroidkeymap-private.h"

struct _GdkAndroidKeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkAndroidKeymap
{
  GdkKeymap parent_instance;

  jobject map;
};

G_DEFINE_TYPE (GdkAndroidKeymap, gdk_android_keymap, GDK_TYPE_KEYMAP)

static PangoDirection
gdk_android_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_android_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_android_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_android_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_android_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_android_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                           guint      keyval,
                                           GArray    *array)
{
  return FALSE;
}

static gboolean
gdk_android_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                            guint          hardware_keycode,
                                            GdkKeymapKey **keys,
                                            guint        **keyvals,
                                            int           *n_entries)
{
  guint normal = gdk_android_keysyms_translate_keycode (hardware_keycode);
  if (normal == GDK_KEY_VoidSymbol)
    return FALSE;
  guint shifted = gdk_android_keysyms_translate_keycode_shifted (hardware_keycode);
  if (normal == shifted)
    {
      if (keys)
        {
          GdkKeymapKey *keys_arr = g_new (GdkKeymapKey, 1);
          keys_arr[0] = (GdkKeymapKey){ .keycode = hardware_keycode, .group = 0, .level = 0 };
          *keys = keys_arr;
        }
      if (keyvals)
        {
          guint *keyvals_arr = g_new (guint, 1);
          keyvals_arr[0] = normal;
          *keyvals = keyvals_arr;
        }
      *n_entries = 1;
      return TRUE;
    }
  else
    {
      if (keys)
        {
          GdkKeymapKey *keys_arr = g_new (GdkKeymapKey, 2);
          keys_arr[0] = (GdkKeymapKey){ .keycode = hardware_keycode, .group = 0, .level = 0 };
          keys_arr[1] = (GdkKeymapKey){ .keycode = hardware_keycode, .group = 0, .level = 1 };
          *keys = keys_arr;
        }
      if (keyvals)
        {
          guint *keyvals_arr = g_new (guint, 2);
          keyvals_arr[0] = normal;
          keyvals_arr[1] = shifted;
          *keyvals = keyvals_arr;
        }
      *n_entries = 2;
      return TRUE;
    }
}

static guint
gdk_android_keymap_lookup_key (GdkKeymap          *keymap,
                               const GdkKeymapKey *key)
{
  g_warn_if_fail (key->level > 1);
  if (key->level >= 1)
    return gdk_android_keysyms_translate_keycode_shifted (key->keycode);
  else
    return gdk_android_keysyms_translate_keycode (key->keycode);
}

static gboolean
gdk_android_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                             guint            hardware_keycode,
                                             GdkModifierType  state,
                                             int              group,
                                             guint           *keyval,
                                             int             *effective_group,
                                             int             *level,
                                             GdkModifierType *consumed_modifiers)
{
  guint skeyval;
  if (state & GDK_SHIFT_MASK)
    {
      skeyval = gdk_android_keysyms_translate_keycode_shifted (hardware_keycode);
      if (skeyval == GDK_KEY_VoidSymbol)
        return FALSE;
      guint unshifted = gdk_android_keysyms_translate_keycode (hardware_keycode);
      if (level)
        *level = (unshifted != skeyval);
      if (consumed_modifiers)
        *consumed_modifiers = (unshifted != skeyval) ? GDK_SHIFT_MASK : 0;
    }
  else
    {
      skeyval = gdk_android_keysyms_translate_keycode (hardware_keycode);
      if (skeyval == GDK_KEY_VoidSymbol)
        return FALSE;
      if (level)
        *level = 0;
      if (consumed_modifiers)
        *consumed_modifiers = 0;
    }
  if (*effective_group)
    *effective_group = 0;

  if (keyval)
    *keyval = skeyval;
  return TRUE;
}

static guint
gdk_android_keymap_get_modifier_state (GdkKeymap *keymap)
{
  return 0;
}

static void
gdk_android_keymap_class_init (GdkAndroidKeymapClass *klass)
{
  GdkKeymapClass *keymap_class = (GdkKeymapClass *) klass;
  keymap_class->get_direction = gdk_android_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_android_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_android_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_android_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_android_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_android_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_android_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_android_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_android_keymap_translate_keyboard_state;
  keymap_class->get_modifier_state = gdk_android_keymap_get_modifier_state;
}

static void
gdk_android_keymap_init (GdkAndroidKeymap *self)
{
}
