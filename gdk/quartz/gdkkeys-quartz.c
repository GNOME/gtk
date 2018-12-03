/* gdkkeys-quartz.c
 *
 * Copyright (C) 2000 Red Hat, Inc.
 * Copyright (C) 2005 Imendio AB
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
/* Some parts of this code come from quartzKeyboard.c,
 * from the Apple X11 Server.
 *
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation files
 *  (the "Software"), to deal in the Software without restriction,
 *  including without limitation the rights to use, copy, modify, merge,
 *  publish, distribute, sublicense, and/or sell copies of the Software,
 *  and to permit persons to whom the Software is furnished to do so,
 *  subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 *  HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 *
 *  Except as contained in this notice, the name(s) of the above
 *  copyright holders shall not be used in advertising or otherwise to
 *  promote the sale, use or other dealings in this Software without
 *  prior written authorization.
 */

#include "config.h"

#include <Carbon/Carbon.h>
#include <AppKit/NSEvent.h>
#include "gdk.h"
#include "gdkquartzkeys.h"
#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"

#define NUM_KEYCODES 128
#define KEYVALS_PER_KEYCODE 4

static GdkKeymap *default_keymap = NULL;

struct _GdkQuartzKeymap
{
  GdkKeymap keymap;
};

struct _GdkQuartzKeymapClass
{
  GdkKeymapClass keymap_class;
};

G_DEFINE_TYPE (GdkQuartzKeymap, gdk_quartz_keymap, GDK_TYPE_KEYMAP)

GdkKeymap *
_gdk_quartz_display_get_keymap (GdkDisplay *display)
{
  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_quartz_keymap_get_type (), NULL);

  return default_keymap;
}

/* This is a table of all keyvals. Each keycode gets KEYVALS_PER_KEYCODE entries.
 * TThere is 1 keyval per modifier (Nothing, Shift, Alt, Shift+Alt);
 */
static guint *keyval_array = NULL;

const static struct {
  guint keycode;
  guint keyval;
  unsigned int modmask; /* So we can tell when a mod key is pressed/released */
} modifier_keys[] = {
  {  54, GDK_KEY_Meta_R,    GDK_QUARTZ_COMMAND_KEY_MASK },
  {  55, GDK_KEY_Meta_L,    GDK_QUARTZ_COMMAND_KEY_MASK },
  {  56, GDK_KEY_Shift_L,   GDK_QUARTZ_SHIFT_KEY_MASK },
  {  57, GDK_KEY_Caps_Lock, GDK_QUARTZ_ALPHA_SHIFT_KEY_MASK },
  {  58, GDK_KEY_Alt_L,     GDK_QUARTZ_ALTERNATE_KEY_MASK },
  {  59, GDK_KEY_Control_L, GDK_QUARTZ_CONTROL_KEY_MASK },
  {  60, GDK_KEY_Shift_R,   GDK_QUARTZ_SHIFT_KEY_MASK },
  {  61, GDK_KEY_Alt_R,     GDK_QUARTZ_ALTERNATE_KEY_MASK },
  {  62, GDK_KEY_Control_R, GDK_QUARTZ_CONTROL_KEY_MASK }
};

const static struct {
  guint keycode;
  guint keyval;
} function_keys[] = {
  { 122, GDK_KEY_F1 },
  { 120, GDK_KEY_F2 },
  {  99, GDK_KEY_F3 },
  { 118, GDK_KEY_F4 },
  {  96, GDK_KEY_F5 },
  {  97, GDK_KEY_F6 },
  {  98, GDK_KEY_F7 },
  { 100, GDK_KEY_F8 },
  { 101, GDK_KEY_F9 },
  { 109, GDK_KEY_F10 },
  { 103, GDK_KEY_F11 },
  { 111, GDK_KEY_F12 },
  { 105, GDK_KEY_F13 },
  { 107, GDK_KEY_F14 },
  { 113, GDK_KEY_F15 },
  { 106, GDK_KEY_F16 }
};

const static struct {
  guint keycode;
  guint normal_keyval, keypad_keyval;
} known_numeric_keys[] = {
  { 65, GDK_KEY_period, GDK_KEY_KP_Decimal },
  { 67, GDK_KEY_asterisk, GDK_KEY_KP_Multiply },
  { 69, GDK_KEY_plus, GDK_KEY_KP_Add },
  { 75, GDK_KEY_slash, GDK_KEY_KP_Divide },
  { 76, GDK_KEY_Return, GDK_KEY_KP_Enter },
  { 78, GDK_KEY_minus, GDK_KEY_KP_Subtract },
  { 81, GDK_KEY_equal, GDK_KEY_KP_Equal },
  { 82, GDK_KEY_0, GDK_KEY_KP_0 },
  { 83, GDK_KEY_1, GDK_KEY_KP_1 },
  { 84, GDK_KEY_2, GDK_KEY_KP_2 },
  { 85, GDK_KEY_3, GDK_KEY_KP_3 },
  { 86, GDK_KEY_4, GDK_KEY_KP_4 },
  { 87, GDK_KEY_5, GDK_KEY_KP_5 },
  { 88, GDK_KEY_6, GDK_KEY_KP_6 },
  { 89, GDK_KEY_7, GDK_KEY_KP_7 },
  { 91, GDK_KEY_8, GDK_KEY_KP_8 },
  { 92, GDK_KEY_9, GDK_KEY_KP_9 }
};

/* These values aren't covered by gdk_unicode_to_keyval */
const static struct {
  gunichar ucs_value;
  guint keyval;
} special_ucs_table [] = {
  { 0x0001, GDK_KEY_Home },
  { 0x0003, GDK_KEY_Return },
  { 0x0004, GDK_KEY_End },
  { 0x0008, GDK_KEY_BackSpace },
  { 0x0009, GDK_KEY_Tab },
  { 0x000b, GDK_KEY_Page_Up },
  { 0x000c, GDK_KEY_Page_Down },
  { 0x000d, GDK_KEY_Return },
  { 0x001b, GDK_KEY_Escape },
  { 0x001c, GDK_KEY_Left },
  { 0x001d, GDK_KEY_Right },
  { 0x001e, GDK_KEY_Up },
  { 0x001f, GDK_KEY_Down },
  { 0x007f, GDK_KEY_Delete },
  { 0xf027, GDK_KEY_dead_acute },
  { 0xf060, GDK_KEY_dead_grave },
  { 0xf300, GDK_KEY_dead_grave },
  { 0xf0b4, GDK_KEY_dead_acute },
  { 0xf301, GDK_KEY_dead_acute },
  { 0xf385, GDK_KEY_dead_acute },
  { 0xf05e, GDK_KEY_dead_circumflex },
  { 0xf2c6, GDK_KEY_dead_circumflex },
  { 0xf302, GDK_KEY_dead_circumflex },
  { 0xf07e, GDK_KEY_dead_tilde },
  { 0xf2dc, GDK_KEY_dead_tilde },
  { 0xf303, GDK_KEY_dead_tilde },
  { 0xf342, GDK_KEY_dead_perispomeni },
  { 0xf0af, GDK_KEY_dead_macron },
  { 0xf304, GDK_KEY_dead_macron },
  { 0xf2d8, GDK_KEY_dead_breve },
  { 0xf306, GDK_KEY_dead_breve },
  { 0xf2d9, GDK_KEY_dead_abovedot },
  { 0xf307, GDK_KEY_dead_abovedot },
  { 0xf0a8, GDK_KEY_dead_diaeresis },
  { 0xf308, GDK_KEY_dead_diaeresis },
  { 0xf2da, GDK_KEY_dead_abovering },
  { 0xf30A, GDK_KEY_dead_abovering },
  { 0xf022, GDK_KEY_dead_doubleacute },
  { 0xf2dd, GDK_KEY_dead_doubleacute },
  { 0xf30B, GDK_KEY_dead_doubleacute },
  { 0xf2c7, GDK_KEY_dead_caron },
  { 0xf30C, GDK_KEY_dead_caron },
  { 0xf0be, GDK_KEY_dead_cedilla },
  { 0xf327, GDK_KEY_dead_cedilla },
  { 0xf2db, GDK_KEY_dead_ogonek },
  { 0xf328, GDK_KEY_dead_ogonek },
  { 0xfe5d, GDK_KEY_dead_iota },
  { 0xf323, GDK_KEY_dead_belowdot },
  { 0xf309, GDK_KEY_dead_hook },
  { 0xf31B, GDK_KEY_dead_horn },
  { 0xf02d, GDK_KEY_dead_stroke },
  { 0xf335, GDK_KEY_dead_stroke },
  { 0xf336, GDK_KEY_dead_stroke },
  { 0xf313, GDK_KEY_dead_abovecomma },
  /*  { 0xf313, GDK_KEY_dead_psili }, */
  { 0xf314, GDK_KEY_dead_abovereversedcomma },
  /*  { 0xf314, GDK_KEY_dead_dasia }, */
  { 0xf30F, GDK_KEY_dead_doublegrave },
  { 0xf325, GDK_KEY_dead_belowring },
  { 0xf2cd, GDK_KEY_dead_belowmacron },
  { 0xf331, GDK_KEY_dead_belowmacron },
  { 0xf32D, GDK_KEY_dead_belowcircumflex },
  { 0xf330, GDK_KEY_dead_belowtilde },
  { 0xf32E, GDK_KEY_dead_belowbreve },
  { 0xf324, GDK_KEY_dead_belowdiaeresis },
  { 0xf311, GDK_KEY_dead_invertedbreve },
  { 0xf02c, GDK_KEY_dead_belowcomma },
  { 0xf326, GDK_KEY_dead_belowcomma }
};

static void
update_keymap (void)
{
  const void *chr_data = NULL;
  guint *p;
  int i;

  /* Note: we could check only if building against the 10.5 SDK instead, but
   * that would make non-xml layouts not work in 32-bit which would be a quite
   * bad regression. This way, old unsupported layouts will just not work in
   * 64-bit.
   */
#ifdef __LP64__
  TISInputSourceRef new_layout = TISCopyCurrentKeyboardLayoutInputSource ();
  CFDataRef layout_data_ref;

#else
  KeyboardLayoutRef new_layout;
  KeyboardLayoutKind layout_kind;

  KLGetCurrentKeyboardLayout (&new_layout);
#endif

  g_free (keyval_array);
  keyval_array = g_new0 (guint, NUM_KEYCODES * KEYVALS_PER_KEYCODE);

#ifdef __LP64__
  layout_data_ref = (CFDataRef) TISGetInputSourceProperty
    (new_layout, kTISPropertyUnicodeKeyLayoutData);

  if (layout_data_ref)
    chr_data = CFDataGetBytePtr (layout_data_ref);

  if (chr_data == NULL)
    {
      g_error ("cannot get keyboard layout data");
      return;
    }
#else

  /* Get the layout kind */
  KLGetKeyboardLayoutProperty (new_layout, kKLKind, (const void **)&layout_kind);

  /* 8-bit-only keyabord layout */
  if (layout_kind == kKLKCHRKind)
    {
      /* Get chr data */
      KLGetKeyboardLayoutProperty (new_layout, kKLKCHRData, (const void **)&chr_data);

      for (i = 0; i < NUM_KEYCODES; i++)
        {
          int j;
          UInt32 modifiers[] = {0, shiftKey, optionKey, shiftKey | optionKey};

          p = keyval_array + i * KEYVALS_PER_KEYCODE;

          for (j = 0; j < KEYVALS_PER_KEYCODE; j++)
            {
              UInt32 c, state = 0;
              UInt16 key_code;
              UniChar uc;

              key_code = modifiers[j] | i;
              c = KeyTranslate (chr_data, key_code, &state);

              if (state != 0)
                {
                  UInt32 state2 = 0;
                  c = KeyTranslate (chr_data, key_code | 128, &state2);
                }

              if (c != 0 && c != 0x10)
                {
                  int k;
                  gboolean found = FALSE;

                  /* FIXME: some keyboard layouts (e.g. Russian) use a
                   * different 8-bit character set. We should check
                   * for this. Not a serious problem, because most
                   * (all?) of these layouts also have a uchr version.
                   */
                  uc = macroman2ucs (c);

                  for (k = 0; k < G_N_ELEMENTS (special_ucs_table); k++)
                    {
                      if (special_ucs_table[k].ucs_value == uc)
                        {
                          p[j] = special_ucs_table[k].keyval;
                          found = TRUE;
                          break;
                        }
                    }

                  /* Special-case shift-tab since GTK+ expects
                   * GDK_KEY_ISO_Left_Tab for that.
                   */
                  if (found && p[j] == GDK_KEY_Tab && modifiers[j] == shiftKey)
                    p[j] = GDK_KEY_ISO_Left_Tab;

                  if (!found)
                    p[j] = gdk_unicode_to_keyval (uc);
                }
            }

          if (p[3] == p[2])
            p[3] = 0;
          if (p[2] == p[1])
            p[2] = 0;
          if (p[1] == p[0])
            p[1] = 0;
          if (p[0] == p[2] &&
              p[1] == p[3])
            p[2] = p[3] = 0;
        }
    }
  /* unicode keyboard layout */
  else if (layout_kind == kKLKCHRuchrKind || layout_kind == kKLuchrKind)
    {
      /* Get chr data */
      KLGetKeyboardLayoutProperty (new_layout, kKLuchrData, (const void **)&chr_data);
#endif

      for (i = 0; i < NUM_KEYCODES; i++)
        {
          int j;
          UInt32 modifiers[] = {0, shiftKey, optionKey, shiftKey | optionKey};
          UniChar chars[4];
          UniCharCount nChars;

          p = keyval_array + i * KEYVALS_PER_KEYCODE;

          for (j = 0; j < KEYVALS_PER_KEYCODE; j++)
            {
              UInt32 state = 0;
              OSStatus err;
              UInt16 key_code;
              UniChar uc;

              key_code = modifiers[j] | i;
              err = UCKeyTranslate (chr_data, i, kUCKeyActionDisplay,
                                    (modifiers[j] >> 8) & 0xFF,
                                    LMGetKbdType(),
                                    0,
                                    &state, 4, &nChars, chars);

              /* FIXME: Theoretically, we can get multiple UTF-16
               * values; we should convert them to proper unicode and
               * figure out whether there are really keyboard layouts
               * that give us more than one character for one
               * keypress.
               */
              if (err == noErr && nChars == 1)
                {
                  int k;
                  gboolean found = FALSE;

                  /* A few <Shift><Option>keys return two characters,
                   * the first of which is U+00a0, which isn't
                   * interesting; so we return the second. More
                   * sophisticated handling is the job of a
                   * GtkIMContext.
                   *
                   * If state isn't zero, it means that it's a dead
                   * key of some sort. Some of those are enumerated in
                   * the special_ucs_table with the high nibble set to
                   * f to push it into the private use range. Here we
                   * do the same.
                   */
                  if (state != 0)
                    chars[nChars - 1] |= 0xf000;
                  uc = chars[nChars - 1];

                  for (k = 0; k < G_N_ELEMENTS (special_ucs_table); k++)
                    {
                      if (special_ucs_table[k].ucs_value == uc)
                        {
                          p[j] = special_ucs_table[k].keyval;
                          found = TRUE;
                          break;
                        }
                    }

                  /* Special-case shift-tab since GTK+ expects
                   * GDK_KEY_ISO_Left_Tab for that.
                   */
                  if (found && p[j] == GDK_KEY_Tab && modifiers[j] == shiftKey)
                    p[j] = GDK_KEY_ISO_Left_Tab;

                  if (!found)
                    p[j] = gdk_unicode_to_keyval (uc);
                }
            }

          if (p[3] == p[2])
            p[3] = 0;
          if (p[2] == p[1])
            p[2] = 0;
          if (p[1] == p[0])
            p[1] = 0;
          if (p[0] == p[2] &&
              p[1] == p[3])
            p[2] = p[3] = 0;
        }
#ifndef __LP64__
    }
  else
    {
      g_error ("unknown type of keyboard layout (neither KCHR nor uchr)"
               " - not supported right now");
    }
#endif

  for (i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      p = keyval_array + modifier_keys[i].keycode * KEYVALS_PER_KEYCODE;

      if (p[0] == 0 && p[1] == 0 &&
          p[2] == 0 && p[3] == 0)
        p[0] = modifier_keys[i].keyval;
    }

  for (i = 0; i < G_N_ELEMENTS (function_keys); i++)
    {
      p = keyval_array + function_keys[i].keycode * KEYVALS_PER_KEYCODE;

      p[0] = function_keys[i].keyval;
      p[1] = p[2] = p[3] = 0;
    }

  for (i = 0; i < G_N_ELEMENTS (known_numeric_keys); i++)
    {
      p = keyval_array + known_numeric_keys[i].keycode * KEYVALS_PER_KEYCODE;

      if (p[0] == known_numeric_keys[i].normal_keyval)
        p[0] = known_numeric_keys[i].keypad_keyval;
    }

  if (default_keymap != NULL)
    g_signal_emit_by_name (default_keymap, "keys-changed");
}

static PangoDirection
gdk_quartz_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_quartz_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  /* FIXME: Can we implement this? */
  return FALSE;
}

static gboolean
gdk_quartz_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  /* FIXME: Implement this. */
  return FALSE;
}

static gboolean
gdk_quartz_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  /* FIXME: Implement this. */
  return FALSE;
}

static gboolean
gdk_quartz_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  /* FIXME: Implement this. */
  return FALSE;
}

static gboolean
gdk_quartz_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                          guint          keyval,
                                          GdkKeymapKey **keys,
                                          gint          *n_keys)
{
  GArray *keys_array;
  int i;

  *n_keys = 0;
  keys_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  for (i = 0; i < NUM_KEYCODES * KEYVALS_PER_KEYCODE; i++)
    {
      GdkKeymapKey key;

      if (keyval_array[i] != keyval)
	continue;

      (*n_keys)++;

      key.keycode = i / KEYVALS_PER_KEYCODE;
      key.group = (i % KEYVALS_PER_KEYCODE) >= 2;
      key.level = i % 2;

      g_array_append_val (keys_array, key);
    }

  *keys = (GdkKeymapKey *)g_array_free (keys_array, FALSE);
  
  return *n_keys > 0;;
}

static gboolean
gdk_quartz_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                           guint          hardware_keycode,
                                           GdkKeymapKey **keys,
                                           guint        **keyvals,
                                           gint          *n_entries)
{
  GArray *keys_array, *keyvals_array;
  int i;
  guint *p;

  *n_entries = 0;

  if (hardware_keycode > NUM_KEYCODES)
    return FALSE;

  if (keys)
    keys_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    keys_array = NULL;

  if (keyvals)
    keyvals_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyvals_array = NULL;

  p = keyval_array + hardware_keycode * KEYVALS_PER_KEYCODE;
  
  for (i = 0; i < KEYVALS_PER_KEYCODE; i++)
    {
      if (!p[i])
	continue;

      (*n_entries)++;
      
      if (keyvals_array)
	g_array_append_val (keyvals_array, p[i]);

      if (keys_array)
	{
	  GdkKeymapKey key;

	  key.keycode = hardware_keycode;
	  key.group = i >= 2;
	  key.level = i % 2;

	  g_array_append_val (keys_array, key);
	}
    }
  
  if (keys)
    *keys = (GdkKeymapKey *)g_array_free (keys_array, FALSE);

  if (keyvals)
    *keyvals = (guint *)g_array_free (keyvals_array, FALSE);

  return *n_entries > 0;
}

#define GET_KEYVAL(keycode, group, level) (keyval_array[(keycode * KEYVALS_PER_KEYCODE + group * 2 + level)])

static guint
gdk_quartz_keymap_lookup_key (GdkKeymap          *keymap,
                              const GdkKeymapKey *key)
{
  return GET_KEYVAL (key->keycode, key->group, key->level);
}

static guint
translate_keysym (guint           hardware_keycode,
		  gint            group,
		  GdkModifierType state,
		  gint           *effective_group,
		  gint           *effective_level)
{
  gint level;
  guint tmp_keyval;

  level = (state & GDK_SHIFT_MASK) ? 1 : 0;

  if (!(GET_KEYVAL (hardware_keycode, group, 0) || GET_KEYVAL (hardware_keycode, group, 1)) &&
      (GET_KEYVAL (hardware_keycode, 0, 0) || GET_KEYVAL (hardware_keycode, 0, 1)))
    group = 0;

  if (!GET_KEYVAL (hardware_keycode, group, level) &&
      GET_KEYVAL (hardware_keycode, group, 0))
    level = 0;

  tmp_keyval = GET_KEYVAL (hardware_keycode, group, level);

  if (state & GDK_LOCK_MASK)
    {
      guint upper = gdk_keyval_to_upper (tmp_keyval);
      if (upper != tmp_keyval)
        tmp_keyval = upper;
    }

  if (effective_group)
    *effective_group = group;
  if (effective_level)
    *effective_level = level;

  return tmp_keyval;
}

static gboolean
gdk_quartz_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                            guint            hardware_keycode,
                                            GdkModifierType  state,
                                            gint             group,
                                            guint           *keyval,
                                            gint            *effective_group,
                                            gint            *level,
                                            GdkModifierType *consumed_modifiers)
{
  guint tmp_keyval;
  GdkModifierType bit;
  guint tmp_modifiers = 0;

  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  if (hardware_keycode < 0 || hardware_keycode >= NUM_KEYCODES)
    return FALSE;

  /* Check if modifiers modify the keyval */
  for (bit = GDK_SHIFT_MASK; bit < GDK_BUTTON1_MASK; bit <<= 1)
    {
      if (translate_keysym (hardware_keycode,
                            (bit == GDK_MOD1_MASK) ? 0 : group,
                            state & ~bit,
                            NULL, NULL) !=
	  translate_keysym (hardware_keycode,
                            (bit == GDK_MOD1_MASK) ? 1 : group,
                            state | bit,
                            NULL, NULL))
	tmp_modifiers |= bit;
    }

  tmp_keyval = translate_keysym (hardware_keycode, group, state, level, effective_group);

  if (consumed_modifiers)
    *consumed_modifiers = tmp_modifiers;

  if (keyval)
    *keyval = tmp_keyval; 

  return TRUE;
}

static void
gdk_quartz_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
                                         GdkModifierType *state)
{
  if (*state & GDK_MOD2_MASK)
    *state |= GDK_META_MASK;
}

static gboolean
gdk_quartz_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                         GdkModifierType *state)
{
  if (*state & GDK_META_MASK)
    *state |= GDK_MOD2_MASK;

  return TRUE;
}

static GdkModifierType
gdk_quartz_keymap_get_modifier_mask (GdkKeymap         *keymap,
                                     GdkModifierIntent  intent)
{
  switch (intent)
    {
    case GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR:
      return GDK_MOD2_MASK;

    case GDK_MODIFIER_INTENT_CONTEXT_MENU:
      return GDK_CONTROL_MASK;

    case GDK_MODIFIER_INTENT_EXTEND_SELECTION:
      return GDK_SHIFT_MASK;

    case GDK_MODIFIER_INTENT_MODIFY_SELECTION:
      return GDK_MOD2_MASK;

    case GDK_MODIFIER_INTENT_NO_TEXT_INPUT:
      return GDK_MOD2_MASK | GDK_CONTROL_MASK;

    case GDK_MODIFIER_INTENT_SHIFT_GROUP:
      return GDK_MOD1_MASK;

    case GDK_MODIFIER_INTENT_DEFAULT_MOD_MASK:
      return (GDK_SHIFT_MASK   | GDK_CONTROL_MASK | GDK_MOD1_MASK    |
	      GDK_MOD2_MASK    | GDK_SUPER_MASK   | GDK_HYPER_MASK   |
	      GDK_META_MASK);

    default:
      g_return_val_if_reached (0);
    }
}

/* What sort of key event is this? Returns one of
 * GDK_KEY_PRESS, GDK_KEY_RELEASE, GDK_NOTHING (should be ignored)
 */
GdkEventType
_gdk_quartz_keys_event_type (NSEvent *event)
{
  unsigned short keycode;
  unsigned int flags;
  int i;
  
  switch ([event type])
    {
    case GDK_QUARTZ_KEY_DOWN:
      return GDK_KEY_PRESS;
    case GDK_QUARTZ_KEY_UP:
      return GDK_KEY_RELEASE;
    case GDK_QUARTZ_FLAGS_CHANGED:
      break;
    default:
      g_assert_not_reached ();
    }
  
  /* For flags-changed events, we have to find the special key that caused the
   * event, and see if it's in the modifier mask. */
  keycode = [event keyCode];
  flags = [event modifierFlags];
  
  for (i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      if (modifier_keys[i].keycode == keycode)
	{
	  if (flags & modifier_keys[i].modmask)
	    return GDK_KEY_PRESS;
	  else
	    return GDK_KEY_RELEASE;
	}
    }
  
  /* Some keypresses (eg: Expose' activations) seem to trigger flags-changed
   * events for no good reason. Ignore them! */
  return GDK_NOTHING;
}

gboolean
_gdk_quartz_keys_is_modifier (guint keycode)
{
  gint i;
  
  for (i = 0; i < G_N_ELEMENTS (modifier_keys); i++)
    {
      if (modifier_keys[i].modmask == 0)
	break;

      if (modifier_keys[i].keycode == keycode)
	return TRUE;
    }

  return FALSE;
}

static void
input_sources_changed_notification (CFNotificationCenterRef  center,
                                    void                    *observer,
                                    CFStringRef              name,
                                    const void              *object,
                                    CFDictionaryRef          userInfo)
{
  update_keymap ();
}

static void
gdk_quartz_keymap_init (GdkQuartzKeymap *keymap)
{
  update_keymap ();
  CFNotificationCenterAddObserver (CFNotificationCenterGetDistributedCenter (),
                                   keymap,
                                   input_sources_changed_notification,
                                   CFSTR ("AppleSelectedInputSourcesChangedNotification"),
                                   NULL,
                                   CFNotificationSuspensionBehaviorDeliverImmediately);
}

static void
gdk_quartz_keymap_finalize (GObject *object)
{
  CFNotificationCenterRemoveObserver (CFNotificationCenterGetDistributedCenter (),
                                      object,
                                      CFSTR ("AppleSelectedInputSourcesChangedNotification"),
                                      NULL);

  G_OBJECT_CLASS (gdk_quartz_keymap_parent_class)->finalize (object);
}

static void
gdk_quartz_keymap_class_init (GdkQuartzKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_quartz_keymap_finalize;

  keymap_class->get_direction = gdk_quartz_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_quartz_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_quartz_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_quartz_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_quartz_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_quartz_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_quartz_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_quartz_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_quartz_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_quartz_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_quartz_keymap_map_virtual_modifiers;
  keymap_class->get_modifier_mask = gdk_quartz_keymap_get_modifier_mask;
}
