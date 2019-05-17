/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include "gdk.h"

#include "gdkprivate-win32.h"
#include "gdkinternals.h"
#include "gdkkeysyms.h"
#include "gdkkeysprivate.h"
#include "gdkwin32keys.h"

enum _GdkWin32KeyLevelState
{
  GDK_WIN32_LEVEL_NONE = 0,
  GDK_WIN32_LEVEL_SHIFT,
  GDK_WIN32_LEVEL_CAPSLOCK,
  GDK_WIN32_LEVEL_SHIFT_CAPSLOCK,
  GDK_WIN32_LEVEL_ALTGR,
  GDK_WIN32_LEVEL_SHIFT_ALTGR,
  GDK_WIN32_LEVEL_CAPSLOCK_ALTGR,
  GDK_WIN32_LEVEL_SHIFT_CAPSLOCK_ALTGR,
  GDK_WIN32_LEVEL_COUNT
};

#define GDK_KEY_UNICODE 0x01000000
#define GDK_KEY_DEAD 0x02000000
#define GDK_KEY_UNIDEAD 0x03000000

typedef enum _GdkWin32KeyLevelState GdkWin32KeyLevelState;

typedef struct _GdkWin32KeyNode GdkWin32KeyNode;

struct _GdkWin32KeyNode
{
  /* Virtual key code */
  guint8                 vk;

  /* Level (i.e. modifiers) with which vk is used. */
  GdkWin32KeyLevelState  level;
};

typedef struct _GdkWin32KeyEntry GdkWin32KeyEntry;

struct _GdkWin32KeyEntry
{
  /* GDK keyval mapped to a virtual key code.
   * If ligature is not NULL, then gdk_keyval is the same as the first
   * keycode in ligature array.
   */
  guint32  gdk_keyval;
  /* A string mapped to a virtual key code. 0-terminated.
   * This is used for ligatures and other things not
   * representable by a single guint32.
   * NULL when the value fits into gdk_keyval.
   */
  guint32 *ligature;
};

typedef struct _GdkWin32CombinationNode GdkWin32CombinationNode;

/* Represents one dead key + another key combination. */
struct _GdkWin32CombinationNode
{
  /* key.vk is the code of the second key in the combination,
   * and key.level is the modifier for it.
   * key.vk is never 0.
   */
  GdkWin32KeyNode  key;

  /* The copy of the keymap key table entry for the key.vk, key.level and
   * the active group. It's here only because looking it up in the keymap
   * key table is not always conveinent.
   */
  GdkWin32KeyEntry entry;

  /* Result of the combination. If this combination produces a chained
   * dead key, the result is its spacing version.
   */
  GdkWin32KeyEntry result;

  /* TRUE = combination produces another dead key.
   * FALSE = combination produces normal characters.
   */
  gboolean         chain;
};

typedef struct _GdkWin32DeadKeyNode GdkWin32DeadKeyNode;

/* We keep one of these for each dead key in layout options.
 * Its "combinations" array is filled with GdkWin32CombinationNode structures,
 * one for each key that can be combined with this dead key.
 * If the combination of this dead key and another key is also a dead key,
 * then a new node is created and added to the dead keys array at the
 * layout options. Because all dead keys end up in the same
 * array, the whole structure only ever has two levels and does not look
 * like a tree. It can be traversed like a tree only because it's possible
 * to alternate between these two levels, always picking a different root
 * node on the even step, until an odd step ends up in a combination result
 * that is not a dead key.
 */
struct _GdkWin32DeadKeyNode
{
  /* key.vk is the code of the key that needs to be pressed to type
   * this dead key. key.level is the needed modifier.
   */
  GdkWin32KeyNode      key;

  /* The spacing version of this dead key.
   * For first-order dead keys it's just a copy of the
   * appropriate key table entry.
   * For chained dead keys this is a copy of the string
   * from the corresponding GdkWin32CombinationNode (looking it up
   * in the tree is inconvenient, since we do not maintain a link
   * to the parent combination after sorting, hence the copy).
   */
  GdkWin32KeyEntry     entry;

  /* Array of GdkWin32CombinationNode, should be sorted by result.gdk_keyval/ligature.
   * Always non-NULL, although might be 0-length.
   */
  GArray              *combinations;

  /* While the tree is being built, we need to repeatedly reproduce the
   * ToUnicodeEx() state, thus we need to remember the keys that must be
   * 'pressed' to achieve this combination.
   * These values are invalid (and set to 0) after sorting.
   */
  GdkWin32DeadKeyNode *parent;
  gsize                combo_index;
};

/*
Example:
  GdkWin32DeadKeyNode
  {
    key.vk = 0xde VK_OEM_7
    key.level = GDK_WIN32_LEVEL_NONE
    entry.gdk_keyval = 0x03000027 GDK_KEY_apostrophe (') | GDK_KEY_UNIDEAD
    combinations =
    {
      GdkWin32CombinationNode
      {
        key.vk = 0xde VK_OEM_7
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x027 GDK_KEY_apostrophe (')
        result.gdk_keyval = 0x1bd GDK_KEY_doubleacute (˝)
        chain = TRUE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x41 VK_A
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x061 GDK_KEY_a (a)
        result.gdk_keyval = 0xe1 GDK_KEY_aacute á
        chain = FALSE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x41 VK_A
        key.level = GDK_WIN32_LEVEL_SHIFT
        from keytable entry: 0x041 GDK_KEY_A (A)
        result.gdk_keyval = 0x0c1 GDK_KEY_Aacute Á
        chain = FALSE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x53 VK_S
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x053 GDK_KEY_S (S)
        result.gdk_keyval = 0x027 0x053 's
        chain = FALSE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x20 VK_SPACE
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x020 GDK_KEY_SPACE ( )
        result.gdk_keyval = 0x027 '
        chain = FALSE
      },
      { ... }
    }
  },
  GdkWin32DeadKeyNode
  {
    key.vk = 0 // Can't be typed directly
    key.level = GDK_WIN32_LEVEL_NONE
    entry.gdk_keyval = 0x030002dd ˝ | GDK_KEY_UNIDEAD
    combinations =
    {
      GdkWin32CombinationNode
      {
        key.vk = 0x41 VK_A
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x061 GDK_KEY_a (a)
        result.ligature = 0x061 0x30b <unicodes as gdk keyvals> a̋
        chain = FALSE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x41 VK_A
        key.level = GDK_WIN32_LEVEL_SHIFT
        from keytable entry: 0x041 GDK_KEY_A (A)
        result.ligature = 0x041 0x30b <unicodes as gdk keyvals> A̋
        chain = FALSE
      },
      GdkWin32CombinationNode
      {
        key.vk = 0x20 VK_SPACE
        key.level = GDK_WIN32_LEVEL_NONE
        from keytable entry: 0x020 GDK_KEY_SPACE ( )
        result.ligature = 0x1bd GDK_KEY_doubleacute (˝)
        chain = FALSE
      },
      { ... }
    }
  },
  ...

Thus:

GDK_KEY_apostrophe + GDK_KEY_a
= GDK_KEY_aacute

GDK_KEY_apostrophe + GDK_KEY_A
= GDK_KEY_Aacute

GDK_KEY_apostrophe + GDK_KEY_s
= GDK_KEY_apostrophe + GDK_KEY_s

GDK_KEY_apostrophe + GDK_KEY_space
= GDK_KEY_apostrophe

GDK_KEY_somethingelse
= GDK_KEY_somethingelse
(W32 API did not provide any deadkey info for GDK_KEY_dead_somethingelse,
 it's copied as-is)

Here's a purely theoretical (keyboard layouts normally available on Windows
do not support dead-key chaining) GDK_KEY_doubleacute example:

GDK_KEY_apostrophe + GDK_KEY_apostrophe
= GDK_KEY_doubleacute

GDK_KEY_apostrophe + GDK_KEY_apostrophe + GDK_KEY_a
= GDK_KEY_doubleacute + GDK_KEY_a
= 0x61 0x30b (a ligature)

GDK_KEY_apostrophe + GDK_KEY_apostrophe + GDK_KEY_s
= GDK_KEY_doubleacute + GDK_KEY_s

GDK_KEY_apostrophe + GDK_KEY_apostrophe + GDK_KEY_space
= GDK_KEY_doubleacute
*/

struct _GdkWin32KeyGroupOptions
{
  /* character that should be used as the decimal separator */
  guint32         decimal_mark;

  /* Scancode for the VK_RSHIFT */
  guint32         scancode_rshift;

  /* TRUE if Ctrl+Alt emulates AltGr */
  gboolean        has_altgr;

  GArray         *dead_keys;
};

typedef struct _GdkWin32KeyGroupOptions GdkWin32KeyGroupOptions;

struct _GdkWin32KeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkWin32Keymap
{
  GdkKeymap parent_instance;

  /* length = what GetKeyboardLayoutList() returns, type = HKL.
   * When it changes, recreate the keymap and repopulate the options.
   */
  GArray *layout_handles;

  /* An array of GdkWin32KeyEntry, mapping Windows virtual key codes
   * to GDK keyvals and strings (for ligatures).
   * Its length is 256 * length(layout_handles) * 2 * 4.
   * 256 is the number of virtual key codes,
   * 2x4 is the number of Shift/AltGr/CapsLock combinations (level),
   * length(layout_handles) is the number of layout handles (group).
   * The structure looks like this:
   * Group 0 at shift level 0 for VK <vk>:
   *    g_array_index (keysym_tab, GdkWin32KeyEntry, vk * group_count * GDK_WIN32_LEVEL_COUNT)
   * Group <group> at shift level 0 for VK <vk>:
   *    g_array_index (keysym_tab, GdkWin32KeyEntry, vk * group_count * GDK_WIN32_LEVEL_COUNT + group * GDK_WIN32_LEVEL_COUNT)
   * or g_array_index (keysym_tab, GdkWin32KeyEntry, (vk * group_count + group) * GDK_WIN32_LEVEL_COUNT)
   * Group <group> at shift level <level> for VK <vk>:
   *    g_array_index (keysym_tab, GdkWin32KeyEntry, vk * group_count * GDK_WIN32_LEVEL_COUNT + group * GDK_WIN32_LEVEL_COUNT + level)
   * or g_array_index (keysym_tab, GdkWin32KeyEntry, (vk * group_count + group) * GDK_WIN32_LEVEL_COUNT + level)
   */
  GArray *keysym_tab;

  /* length = length(layout_handles), type =  GdkWin32KeyGroupOptions
   * Kept separate from layout_handles because layout_handles is
   * populated by W32 API.
   */
  GArray *options;

  /* Index of a handle in layout_handles,
   * at any point it should be the same handle as GetKeyboardLayout(0) returns,
   * but GDK caches it to avoid calling GetKeyboardLayout (0) every time.
   */
  guint8 active_layout;
};

#define keyentry(_table, _group_count, _v_keycode, _group, _level) (&g_array_index (_table, GdkWin32KeyEntry, (_v_keycode * _group_count + _group) * GDK_WIN32_LEVEL_COUNT + _level))

G_DEFINE_TYPE (GdkWin32Keymap, gdk_win32_keymap, GDK_TYPE_KEYMAP)

guint _gdk_keymap_serial = 0;

static GdkKeymap *default_keymap = NULL;

#define KEY_STATE_SIZE 256

static void update_keymap (GdkKeymap *gdk_keymap);

static void
gdk_win32_key_group_options_clear (GdkWin32KeyGroupOptions *options)
{
  g_clear_pointer (&options->dead_keys, g_array_unref);
}

static void
gdk_win32_key_entry_clear (GdkWin32KeyEntry *entry)
{
  g_clear_pointer (&entry->ligature, g_free);
}

static void
gdk_win32_key_entries_clear (GArray *entries)
{
  gint i;

  for (i = 0; i < entries->len; i++)
    gdk_win32_key_entry_clear (&g_array_index (entries, GdkWin32KeyEntry, i));
}

static void
gdk_win32_dead_key_node_clear (GdkWin32DeadKeyNode *node)
{
  gdk_win32_key_entry_clear (&node->entry);
  g_clear_pointer (&node->combinations, g_array_unref);
}

static void
gdk_win32_combination_node_clear (GdkWin32CombinationNode *node)
{
  gdk_win32_key_entry_clear (&node->result);
  gdk_win32_key_entry_clear (&node->entry);
}

static void
gdk_win32_keymap_init (GdkWin32Keymap *keymap)
{
  keymap->layout_handles = g_array_new (FALSE, FALSE, sizeof (HKL));
  keymap->options = g_array_new (FALSE, FALSE, sizeof (GdkWin32KeyGroupOptions));
  g_array_set_clear_func (keymap->options, (GDestroyNotify) gdk_win32_key_group_options_clear);
  keymap->keysym_tab = g_array_new (FALSE, FALSE, sizeof (GdkWin32KeyEntry));
  g_array_set_clear_func (keymap->keysym_tab, (GDestroyNotify) gdk_win32_key_entry_clear);
  keymap->active_layout = 0;
  update_keymap (GDK_KEYMAP (keymap));
}

static void
gdk_win32_keymap_finalize (GObject *object)
{
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (object);

  g_clear_pointer (&keymap->keysym_tab, g_array_unref);
  g_clear_pointer (&keymap->layout_handles, g_array_unref);
  g_clear_pointer (&keymap->options, g_array_unref);

  G_OBJECT_CLASS (gdk_win32_keymap_parent_class)->finalize (object);
}

#ifdef G_ENABLE_DEBUG
static void
print_keysym_tab (GdkWin32Keymap *keymap)
{
  gint                      li;
  GdkWin32KeyGroupOptions  *options;
  gint                      vk;
  GdkWin32KeyLevelState     level;
  gint                      group_size = keymap->layout_handles->len;

  for (li = 0; li < group_size; li++)
    {
      options = &g_array_index (keymap->options, GdkWin32KeyGroupOptions, li);
      g_print ("keymap %d (0x%p):%s\n",
               li, g_array_index (keymap->layout_handles, HKL, li),
               options->has_altgr ? " (uses AltGr)" : "");

      for (vk = 0; vk < KEY_STATE_SIZE; vk++)
        {
          g_print ("%#.02x: ", vk);

          for (level = 0; level < GDK_WIN32_LEVEL_COUNT; level++)
            {
              const gchar *name;
              GdkWin32KeyEntry *entry = keyentry (keymap->keysym_tab, group_size, vk, li, level);

              name = gdk_keyval_name (entry->gdk_keyval);

              g_print ("%s ", name ? name : "(none)");
            }

          g_print ("\n");
        }
    }
}
#endif

static void
handle_special (guint  vk,
		guint *ksymp,
		gint   shift)

{
  switch (vk)
    {
    case VK_CANCEL:
      *ksymp = GDK_KEY_Cancel; break;
    case VK_BACK:
      *ksymp = GDK_KEY_BackSpace; break;
    case VK_TAB:
      if (shift & 0x1)
	*ksymp = GDK_KEY_ISO_Left_Tab;
      else
	*ksymp = GDK_KEY_Tab;
      break;
    case VK_CLEAR:
      *ksymp = GDK_KEY_Clear; break;
    case VK_RETURN:
      *ksymp = GDK_KEY_Return; break;
    case VK_SHIFT:
    case VK_LSHIFT:
      *ksymp = GDK_KEY_Shift_L; break;
    case VK_CONTROL:
    case VK_LCONTROL:
      *ksymp = GDK_KEY_Control_L; break;
    case VK_MENU:
    case VK_LMENU:
      *ksymp = GDK_KEY_Alt_L; break;
    case VK_PAUSE:
      *ksymp = GDK_KEY_Pause; break;
    case VK_ESCAPE:
      *ksymp = GDK_KEY_Escape; break;
    case VK_PRIOR:
      *ksymp = GDK_KEY_Prior; break;
    case VK_NEXT:
      *ksymp = GDK_KEY_Next; break;
    case VK_END:
      *ksymp = GDK_KEY_End; break;
    case VK_HOME:
      *ksymp = GDK_KEY_Home; break;
    case VK_LEFT:
      *ksymp = GDK_KEY_Left; break;
    case VK_UP:
      *ksymp = GDK_KEY_Up; break;
    case VK_RIGHT:
      *ksymp = GDK_KEY_Right; break;
    case VK_DOWN:
      *ksymp = GDK_KEY_Down; break;
    case VK_SELECT:
      *ksymp = GDK_KEY_Select; break;
    case VK_PRINT:
      *ksymp = GDK_KEY_Print; break;
    case VK_SNAPSHOT:
      *ksymp = GDK_KEY_Print; break;
    case VK_EXECUTE:
      *ksymp = GDK_KEY_Execute; break;
    case VK_INSERT:
      *ksymp = GDK_KEY_Insert; break;
    case VK_DELETE:
      *ksymp = GDK_KEY_Delete; break;
    case VK_HELP:
      *ksymp = GDK_KEY_Help; break;
    case VK_LWIN:
      *ksymp = GDK_KEY_Meta_L; break;
    case VK_RWIN:
      *ksymp = GDK_KEY_Meta_R; break;
    case VK_APPS:
      *ksymp = GDK_KEY_Menu; break;
    case VK_DECIMAL:
      *ksymp = GDK_KEY_KP_Decimal; break;
    case VK_MULTIPLY:
      *ksymp = GDK_KEY_KP_Multiply; break;
    case VK_ADD:
      *ksymp = GDK_KEY_KP_Add; break;
    case VK_SEPARATOR:
      *ksymp = GDK_KEY_KP_Separator; break;
    case VK_SUBTRACT:
      *ksymp = GDK_KEY_KP_Subtract; break;
    case VK_DIVIDE:
      *ksymp = GDK_KEY_KP_Divide; break;
    case VK_NUMPAD0:
      *ksymp = GDK_KEY_KP_0; break;
    case VK_NUMPAD1:
      *ksymp = GDK_KEY_KP_1; break;
    case VK_NUMPAD2:
      *ksymp = GDK_KEY_KP_2; break;
    case VK_NUMPAD3:
      *ksymp = GDK_KEY_KP_3; break;
    case VK_NUMPAD4:
      *ksymp = GDK_KEY_KP_4; break;
    case VK_NUMPAD5:
      *ksymp = GDK_KEY_KP_5; break;
    case VK_NUMPAD6:
      *ksymp = GDK_KEY_KP_6; break;
    case VK_NUMPAD7:
      *ksymp = GDK_KEY_KP_7; break;
    case VK_NUMPAD8:
      *ksymp = GDK_KEY_KP_8; break;
    case VK_NUMPAD9:
      *ksymp = GDK_KEY_KP_9; break;
    case VK_F1:
      *ksymp = GDK_KEY_F1; break;
    case VK_F2:
      *ksymp = GDK_KEY_F2; break;
    case VK_F3:
      *ksymp = GDK_KEY_F3; break;
    case VK_F4:
      *ksymp = GDK_KEY_F4; break;
    case VK_F5:
      *ksymp = GDK_KEY_F5; break;
    case VK_F6:
      *ksymp = GDK_KEY_F6; break;
    case VK_F7:
      *ksymp = GDK_KEY_F7; break;
    case VK_F8:
      *ksymp = GDK_KEY_F8; break;
    case VK_F9:
      *ksymp = GDK_KEY_F9; break;
    case VK_F10:
      *ksymp = GDK_KEY_F10; break;
    case VK_F11:
      *ksymp = GDK_KEY_F11; break;
    case VK_F12:
      *ksymp = GDK_KEY_F12; break;
    case VK_F13:
      *ksymp = GDK_KEY_F13; break;
    case VK_F14:
      *ksymp = GDK_KEY_F14; break;
    case VK_F15:
      *ksymp = GDK_KEY_F15; break;
    case VK_F16:
      *ksymp = GDK_KEY_F16; break;
    case VK_F17:
      *ksymp = GDK_KEY_F17; break;
    case VK_F18:
      *ksymp = GDK_KEY_F18; break;
    case VK_F19:
      *ksymp = GDK_KEY_F19; break;
    case VK_F20:
      *ksymp = GDK_KEY_F20; break;
    case VK_F21:
      *ksymp = GDK_KEY_F21; break;
    case VK_F22:
      *ksymp = GDK_KEY_F22; break;
    case VK_F23:
      *ksymp = GDK_KEY_F23; break;
    case VK_F24:
      *ksymp = GDK_KEY_F24; break;
    case VK_NUMLOCK:
      *ksymp = GDK_KEY_Num_Lock; break;
    case VK_SCROLL:
      *ksymp = GDK_KEY_Scroll_Lock; break;
    case VK_RSHIFT:
      *ksymp = GDK_KEY_Shift_R; break;
    case VK_RCONTROL:
      *ksymp = GDK_KEY_Control_R; break;
    case VK_RMENU:
      *ksymp = GDK_KEY_Alt_R; break;
    }
}

static void
set_level_vks (guchar               *key_state,
	       GdkWin32KeyLevelState level)
{
  switch (level)
    {
    case GDK_WIN32_LEVEL_NONE:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CAPITAL] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case GDK_WIN32_LEVEL_SHIFT:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CAPITAL] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case GDK_WIN32_LEVEL_CAPSLOCK:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CAPITAL] = 0x01;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case GDK_WIN32_LEVEL_SHIFT_CAPSLOCK:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CAPITAL] = 0x01;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case GDK_WIN32_LEVEL_ALTGR:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CAPITAL] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    case GDK_WIN32_LEVEL_SHIFT_ALTGR:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CAPITAL] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    case GDK_WIN32_LEVEL_CAPSLOCK_ALTGR:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CAPITAL] = 0x01;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    case GDK_WIN32_LEVEL_SHIFT_CAPSLOCK_ALTGR:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CAPITAL] = 0x01;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    case GDK_WIN32_LEVEL_COUNT:
      g_assert_not_reached ();
      break;
    }
}

static void
reset_after_dead (guchar key_state[KEY_STATE_SIZE],
                  HKL    handle)
{
  guchar  temp_key_state[KEY_STATE_SIZE];
  wchar_t wcs[17];

  memmove (temp_key_state, key_state, KEY_STATE_SIZE);

  temp_key_state[VK_SHIFT] =
    temp_key_state[VK_CONTROL] =
    temp_key_state[VK_CAPITAL] =
    temp_key_state[VK_MENU] = 0;

  ToUnicodeEx (VK_SPACE, MapVirtualKey (VK_SPACE, 0),
	       temp_key_state, wcs, G_N_ELEMENTS (wcs),
	       0, handle);
}

/* keypad decimal mark depends on active keyboard layout
 * return current decimal mark as unicode character
 */
guint32
_gdk_win32_keymap_get_decimal_mark (GdkWin32Keymap *keymap)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0 &&
      g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).decimal_mark)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).decimal_mark;

  return (guint32) '.';
}

static gboolean
layouts_are_the_same (GArray *array, HKL *hkls, gint hkls_len)
{
  gint i;

  if (hkls_len != array->len)
    return FALSE;

  for (i = 0; i < hkls_len; i++)
    if (hkls[i] != g_array_index (array, HKL, i))
      return FALSE;

  return TRUE;
}

static void
check_that_active_layout_is_in_sync (GdkWin32Keymap *keymap)
{
  HKL     hkl;
  HKL     cached_hkl;
  wchar_t hkl_name[KL_NAMELENGTH];

  if (keymap->layout_handles->len <= 0)
    return;

  hkl = GetKeyboardLayout (0);
  cached_hkl = g_array_index (keymap->layout_handles, HKL, keymap->active_layout);

  if (hkl != cached_hkl)
    {
      if (!GetKeyboardLayoutNameW (hkl_name))
        wcscpy_s (hkl_name, KL_NAMELENGTH, L"(NULL)");

      g_warning ("Cached active layout #%d (0x%p) does not match actual layout %S, 0x%p",
                 keymap->active_layout, cached_hkl, hkl_name, hkl);
    }
}

static gint
sort_dead_keys_by_gdk_keyval (gconstpointer a,
                              gconstpointer b)
{
  const GdkWin32DeadKeyNode *one = a;
  const GdkWin32DeadKeyNode *two = b;
  guint i;

  if (one->entry.gdk_keyval < two->entry.gdk_keyval)
    return -1;
  else if (one->entry.gdk_keyval > two->entry.gdk_keyval)
    return 1;

  if (one->entry.ligature == NULL && two->entry.ligature != NULL)
    return -1;
  else if (one->entry.ligature != NULL && two->entry.ligature == NULL)
    return 1;

  for (i = 1; ; i++)
    {
      if (one->entry.ligature[i] < two->entry.ligature[i])
        return -1;
      else if (one->entry.ligature[i] > two->entry.ligature[i])
        return 1;
      if (one->entry.ligature[i] == 0)
        break;
    }

  return 0;
}

static gint
sort_combinations_by_gdk_keyval (gconstpointer a,
                                 gconstpointer b)
{
  const GdkWin32CombinationNode *one = a;
  const GdkWin32CombinationNode *two = b;
  guint i;

  if (one->entry.gdk_keyval < two->entry.gdk_keyval)
    return -1;
  else if (one->entry.gdk_keyval > two->entry.gdk_keyval)
    return 1;

  if (one->entry.ligature == NULL && two->entry.ligature != NULL)
    return -1;
  else if (one->entry.ligature != NULL && two->entry.ligature == NULL)
    return 1;
  else if (one->entry.ligature == NULL && two->entry.ligature == NULL)
    return 0;

  for (i = 1; ; i++)
    {
      if (one->entry.ligature[i] < two->entry.ligature[i])
        return -1;
      else if (one->entry.ligature[i] > two->entry.ligature[i])
        return 1;
      if (one->entry.ligature[i] == 0)
        break;
    }

  return 0;
}

static gboolean
linear_find_dead_key_by_keyval (GArray   *array,
                                guint32   keyval,
                                gsize    *index)
{
  gsize i;
  GdkWin32DeadKeyNode *dead_key;

  for (i = 0; i < array->len; i++)
    {
      dead_key = &g_array_index (array, GdkWin32DeadKeyNode, i);
      if (dead_key->entry.gdk_keyval == keyval)
        return TRUE;
    }

  return FALSE;
}

static gboolean
find_dead_key_by_keyval (GArray   *array,
                         guint32   keyval,
                         gsize    *index)
{
  gsize i;
  gsize i_max;
  GdkWin32DeadKeyNode *dead_key, *dead_key_max;

  if (array->len == 0)
    return FALSE;

  i = 0;
  i_max = array->len - 1;

  while (i != i_max)
    {
      gsize middle;

      dead_key = &g_array_index (array, GdkWin32DeadKeyNode, i);
      dead_key_max = &g_array_index (array, GdkWin32DeadKeyNode, i_max);

      /* It's unknown whether a deadkey can or can't occupy more than one codepoint.
       * For now assume that it can't.
       */
      if (dead_key->entry.gdk_keyval == keyval)
        {
          break;
        }
      else if (dead_key_max->entry.gdk_keyval == keyval)
        {
          i = i_max;
          break;
        }
      else if (i + 1 == i_max)
        {
          break;
        }

      middle = i + (i_max - i) / 2;
      dead_key = &g_array_index (array, GdkWin32DeadKeyNode, middle);

      if (dead_key->entry.gdk_keyval < keyval)
        i = middle;
      else if (dead_key->entry.gdk_keyval > keyval)
        i_max = middle;
      else
        i = i_max = middle;
    }

  dead_key = &g_array_index (array, GdkWin32DeadKeyNode, i);

  if (dead_key->entry.gdk_keyval != keyval)
    return FALSE;

  *index = i;

  return TRUE;
}

static gboolean
find_combination_by_keyval (GdkWin32Keymap *keymap,
                            GArray         *array,
                            guint32         keyval,
                            gsize          *index)
{
  gsize  i;
  gsize  i_max;
  GdkWin32CombinationNode *node, *node_max;

  if (array->len == 0)
    return FALSE;

  i = 0;
  i_max = array->len - 1;

  while (i != i_max)
    {
      gsize middle;

      node = &g_array_index (array, GdkWin32CombinationNode, i);
      node_max = &g_array_index (array, GdkWin32CombinationNode, i_max);

      /* It's unknown whether a deadkey can or can't occupy more than one codepoint.
       * For now assume that it can't.
       */
      if (node->entry.gdk_keyval == keyval)
        {
          break;
        }
      else if (node_max->entry.gdk_keyval == keyval)
        {
          i = i_max;
          break;
        }
      else if (i + 1 == i_max)
        {
          break;
        }

      middle = i + (i_max - i) / 2;
      node = &g_array_index (array, GdkWin32CombinationNode, middle);

      if (node->entry.gdk_keyval < keyval)
        i = middle;
      else if (node->entry.gdk_keyval > keyval)
        i_max = middle;
      else
        i = i_max = middle;
    }

  node = &g_array_index (array, GdkWin32CombinationNode, i);

  if (node->entry.gdk_keyval != keyval)
    return FALSE;

  *index = i;

  return TRUE;
}

static void
update_keymap (GdkKeymap *gdk_keymap)
{
  int                      hkls_len;
  static int               hkls_size = 0;
  static HKL              *hkls = NULL;
  gboolean                 no_list;
  static guint             current_serial = 0;
  gint                     i, group;
  GdkWin32KeyLevelState    level;
  GdkWin32KeyGroupOptions *options;
  GdkWin32Keymap          *keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  gint                     keysym_tab_size;

  guchar                   key_state[KEY_STATE_SIZE];
  guint                    scancode;
  guint                    vk;

  if (keymap->keysym_tab->len != 0 &&
      current_serial == _gdk_keymap_serial)
    return;

  no_list = FALSE;
  hkls_len = GetKeyboardLayoutList (0, NULL);

  if (hkls_len <= 0)
    {
      hkls_len = 1;
      no_list = TRUE;
    }
  else if (hkls_len > 255)
    {
      hkls_len = 255;
    }

  if (hkls_size < hkls_len)
    {
      hkls = g_renew (HKL, hkls, hkls_len);
      hkls_size = hkls_len;
    }

  if (hkls_len != GetKeyboardLayoutList (hkls_len, hkls))
    {
      if (!no_list)
        return;

      hkls[0] = GetKeyboardLayout (0);
      hkls_len = 1;
    }

  if (layouts_are_the_same (keymap->layout_handles, hkls, hkls_len))
    {
      check_that_active_layout_is_in_sync (keymap);
      current_serial = _gdk_keymap_serial;

      return;
    }

  GDK_NOTE (EVENTS, g_print ("\nHave %d keyboard layouts:", hkls_len));

  for (i = 0; i < hkls_len; i++)
    {
      GDK_NOTE (EVENTS, g_print (" 0x%p", hkls[i]));

      if (GetKeyboardLayout (0) == hkls[i])
        {
          wchar_t hkl_name[KL_NAMELENGTH];

          if (!GetKeyboardLayoutNameW (hkl_name))
            wcscpy_s (hkl_name, KL_NAMELENGTH, L"(NULL)");

          GDK_NOTE (EVENTS, g_print ("(active, %S)", hkl_name));
        }
    }

  GDK_NOTE (EVENTS, g_print ("\n"));

  keysym_tab_size = hkls_len * 256 * 2 * 4;

  gdk_win32_key_entries_clear (keymap->keysym_tab);
  g_array_set_size (keymap->keysym_tab, keysym_tab_size);

  for (i = 0; i < keysym_tab_size; i++)
    {
      GdkWin32KeyEntry *entry = &g_array_index (keymap->keysym_tab, GdkWin32KeyEntry, i);

      entry->gdk_keyval = 0;
      entry->ligature = NULL;
    }

  g_array_set_size (keymap->layout_handles, hkls_len);
  g_array_set_size (keymap->options, hkls_len);

  for (i = 0; i < hkls_len; i++)
    {
      options = &g_array_index (keymap->options, GdkWin32KeyGroupOptions, i);

      options->decimal_mark = 0;
      options->scancode_rshift = 0;
      options->has_altgr = FALSE;
      options->dead_keys = g_array_new (FALSE, FALSE, sizeof (GdkWin32DeadKeyNode));
      g_array_set_clear_func (options->dead_keys, (GDestroyNotify) gdk_win32_dead_key_node_clear);

      g_array_index (keymap->layout_handles, HKL, i) = hkls[i];

      if (hkls[i] == _gdk_input_locale)
        keymap->active_layout = i;
    }

  for (vk = 0; vk < KEY_STATE_SIZE; vk++)
    {
      for (group = 0; group < hkls_len; group++)
        {
          GdkWin32KeyEntry *keygroup = keyentry (keymap->keysym_tab, hkls_len, vk, group, 0);

          options = &g_array_index (keymap->options, GdkWin32KeyGroupOptions, group);
          scancode = MapVirtualKeyEx (vk, 0, hkls[group]);

          /* MapVirtualKeyEx() fails to produce a scancode for VK_DIVIDE and VK_PAUSE.
           * Ignore that, handle_special() will figure out a Gdk keyval for these
           * without needing a scancode.
           */
          if (scancode == 0 &&
              vk != VK_DIVIDE &&
              vk != VK_PAUSE)
            {
              for (level = GDK_WIN32_LEVEL_NONE; level < GDK_WIN32_LEVEL_COUNT; level++)
                keygroup[level].gdk_keyval = GDK_KEY_VoidSymbol;

              continue;
            }

          if (vk == VK_RSHIFT)
            options->scancode_rshift = scancode;

          key_state[vk] = 0x80;

          for (level = GDK_WIN32_LEVEL_NONE; level < GDK_WIN32_LEVEL_COUNT; level++)
            {
              GdkWin32KeyEntry *ksymp = &keygroup[level];

              set_level_vks (key_state, level);

              ksymp->gdk_keyval = 0;

              /* First, handle those virtual keys that we always want
               * as special GDK_* keysyms, even if ToUnicodeEx might
               * turn some them into a Unicode character (like TAB and
               * ESC).
               */
              handle_special (vk, &ksymp->gdk_keyval, level);

              /* handle_special() produced a gdk keyval for VK_DECIMAL,
               * but we still want to know which unicode character it should
               * correspond to.
               */
              if ((ksymp->gdk_keyval == 0) ||
                  ((vk == VK_DECIMAL) && (level == GDK_WIN32_LEVEL_NONE)))
                {
                  /* Note: ToUnicodeEx() doesn't tell us how much space it needs
                   * to store the output. Nothing anywhere says that it can't
                   * be longer than 16 wchars. If it does happen to be longer,
                   * then this will cause the output to be truncated.
                   * A workaround for this is to call ToUnicodeEx() repeatedly
                   * with larger and larger buffer, until its positive return value
                   * becomes less than the length of the buffer. That's very
                   * awkward, so we'll burn this bridge after we cross it.
                   */
                  wchar_t              wcs[17];
                  gint                 k;
                  GdkWin32DeadKeyNode  dead_key;
                  gsize                existing_deadkey_i;
                  gunichar            *ucs4 = NULL;
                  glong                ucs4_chars = 0;

                  memset (wcs, 0, G_N_ELEMENTS (wcs) * sizeof (wcs[0]));
                  k = ToUnicodeEx (vk, scancode, key_state,
                                   wcs, G_N_ELEMENTS (wcs),
                                   0, hkls[group]);
                  wcs[G_N_ELEMENTS (wcs) - 1] = 0;
                  /* These checks are spearate to reduce if{} nesting */
                  if (k == -1 || k > 0)
                    {
                      /* Dead key or normal output, convert the string it produces */
                      GError *error = NULL;
                      ucs4 = g_utf16_to_ucs4 (wcs, k, NULL, &ucs4_chars, &error);

                      if (ucs4 == NULL)
                        {
                          g_warning ("Failed to convert %d UTF-16 code points to UCS-4: %s",
                                     k, error->message);
                          g_clear_error (&error);
                        }
                    }

                  if ((k == -1 || k > 0) && ucs4 != NULL)
                    {
                      /* Dead key or normal output, with a valid string */
                      gsize ucs4_index;

                      /* Remember what the decimal separator looks like.
                       * Or just store the first keyval.
                       */
                      if ((vk == VK_DECIMAL) && (level == GDK_WIN32_LEVEL_NONE))
                        options->decimal_mark = ucs4[0];
                      else
                        /* Be sure to keep the unicode value for dead keys, don't call gdk_unicode_to_keyval() */
                        ksymp->gdk_keyval = k == -1 ? (ucs4[0] | GDK_KEY_UNIDEAD) : gdk_unicode_to_keyval (ucs4[0]);

                      /* We store GDK keyvals, not unicode characters */
                      for (ucs4_index = 0; ucs4_index < ucs4_chars; ucs4_index++)
                         ucs4[ucs4_index] = k == -1 ? (ucs4[ucs4_index] | GDK_KEY_UNIDEAD) : gdk_unicode_to_keyval (ucs4[ucs4_index]);

                      if (ucs4_chars == 1)
                        {
                          /* Everything fits into one uint32 */
                          ksymp->ligature = NULL;
                          g_free (ucs4);
                        }
                      else
                        {
                          /* Keep theligature array around */
                          ksymp->ligature = ucs4;
                        }
                    }

                  if ((k == -1) &&
                      ucs4 != NULL &&
                      !linear_find_dead_key_by_keyval (options->dead_keys, ksymp->gdk_keyval, &existing_deadkey_i))
                    {
                      /* Dead key, with a valid string, and it isn't already added to the list*/
                      dead_key.key.vk = vk;
                      dead_key.key.level = level;
                      dead_key.entry.gdk_keyval = ksymp->gdk_keyval;
                      dead_key.entry.ligature = NULL;

                      if (ucs4_chars > 1)
                        {
                          /* Also needs to keep the ligature array around */
                          dead_key.entry.ligature = g_new0 (guint32, ucs4_chars + 1);
                          memcpy (dead_key.entry.ligature, ksymp->ligature, ucs4_chars * sizeof (guint32));
                        }

                      dead_key.combinations = g_array_new (FALSE, FALSE, sizeof (GdkWin32CombinationNode));
                      g_array_set_clear_func (dead_key.combinations, (GDestroyNotify) gdk_win32_combination_node_clear);
                      dead_key.parent = NULL;
                      dead_key.combo_index = 0;
                      g_array_append_val (options->dead_keys, dead_key);
                    }

                  if (k < -1)
                    {
                      /* This case is undocumented */
                      g_warning ("ToUnicodeEx() returned %d for keycode %u in group %u at level %u",
                                 k, vk, group, level);
                    }

                  /* If it is a dead key, then it has been stored in
                   * the keyboard layout's state by
                   * ToUnicodeEx(). Yes, this is an
                   * incredibly silly API! Make the keyboard
                   * layout forget it by calling
                   * ToUnicodeEx() once more, with the
                   * virtual key code and scancode for the
                   * spacebar, without shift or AltGr. Otherwise
                   * the next call to ToUnicodeEx() with a different
                   * key would try to combine with the dead key.
                   * We probably lose nothing by doing this here unconditionally
                   */
                  reset_after_dead (key_state, hkls[group]);
                }

              if (ksymp->gdk_keyval == 0)
                ksymp->gdk_keyval = GDK_KEY_VoidSymbol;
            }

          key_state[vk] = 0;

          /* Check if keyboard has an AltGr key by checking if
           * the mapping with Control+Alt is different.
           * Don't test CapsLock here, as it does not seem to affect
           * dead keys themselves, only the results of dead key combinations.
           */
          if (!options->has_altgr)
            if ((keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol &&
                 keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval) ||
                (keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol &&
                 keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval != keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval))
              options->has_altgr = TRUE;
        }
    }

  scancode = 0x0;

  /* Now loop through all the dead keys and figure out what they combine with. */
  for (group = 0; group < hkls_len; group++)
    {
      options = &g_array_index (keymap->options, GdkWin32KeyGroupOptions, group);

      for (i = 0; i < options->dead_keys->len; i++)
        {
          GdkWin32DeadKeyNode *dead_key;
          GdkWin32DeadKeyNode *p;
          GList           *stack = NULL;

          dead_key = &g_array_index (options->dead_keys, GdkWin32DeadKeyNode, i);

          /* For chained dead keys we need to call ToUnicodeEx() multiple
           * times in sequence to change it to the state where we can test
           * a particular dead key. Keep a stack of dead keys around
           * for this purpose. The "parent" and "combo_index" members
           * exist solely because of that.
           */
          p = dead_key;

          do
            {
              stack = g_list_prepend (stack, p);
              p = p->parent;
            } while (p != NULL);

          for (vk = 0; vk < KEY_STATE_SIZE; vk++)
            {
              /* VK_PACKET never combines with anything in any meaningful way */
              if (vk == VK_PACKET)
                continue;

              for (level = GDK_WIN32_LEVEL_NONE; level < GDK_WIN32_LEVEL_COUNT; level++)
                {
                  wchar_t                  wcs[17];
                  gint                     k;
                  GdkWin32CombinationNode  combo;
                  gsize                    existing_deadkey_i;
                  GdkWin32DeadKeyNode      chained_dead_key;
                  gunichar                *ucs4 = NULL;
                  glong                    ucs4_chars;
                  gboolean                 primed = TRUE;
                  GList                   *stack_p;

                  for (stack_p = stack; stack_p != NULL;)
                    {
                      GdkWin32DeadKeyNode *stack_dead_key = (GdkWin32DeadKeyNode *) stack_p->data;

                      /* Prime the ToUnicodeEx() internal state */
                      set_level_vks (key_state, stack_dead_key->key.level);
                      scancode = MapVirtualKeyEx (stack_dead_key->key.vk, 0, hkls[group]);
                      key_state[stack_dead_key->key.vk] = 0x80;
                      k = ToUnicodeEx (stack_dead_key->key.vk, scancode, key_state,
                                       wcs, G_N_ELEMENTS (wcs),
                                       0, hkls[group]);
                      key_state[stack_dead_key->key.vk] = 0;

                      stack_p = stack_p->next;

                      switch (k)
                        {
                        case -1:
                          /* Okay */
                          break;
                        default:
                          /* Expected a dead key, got something else */
                          reset_after_dead (key_state, hkls[group]);
                          primed = FALSE;
                          break;
                        }
                    }

                  if (!primed)
                    {
                      reset_after_dead (key_state, hkls[group]);
                      continue;
                    }

                  /* Check how it combines with vk */
                  memset (wcs, 0, G_N_ELEMENTS (wcs) * sizeof (wcs[0]));
                  set_level_vks (key_state, level);
                  scancode = MapVirtualKeyEx (vk, 0, hkls[group]);
                  key_state[vk] = 0x80;
                  k = ToUnicodeEx (vk, scancode, key_state,
                                   wcs, G_N_ELEMENTS (wcs),
                                   0, hkls[group]);
                  wcs[G_N_ELEMENTS (wcs) - 1] = 0;
                  key_state[vk] = 0;

                  if (k == -1 && wcs[0] == 0)
                    {
                      /* Abnormal layout that does not produce spacing version
                       * of the dead key. Add a space to force it to give
                       * some output.
                       */
                      gsize index;
                      set_level_vks (key_state, 0);
                      scancode = MapVirtualKeyEx (vk, 0, hkls[group]);
                      key_state[VK_SPACE] = 0x80;
                      k = ToUnicodeEx (VK_SPACE, scancode, key_state,
                                       wcs, G_N_ELEMENTS (wcs),
                                       0, hkls[group]);
                      key_state[VK_SPACE] = 0;
                      if (k > 0)
                        {
                          for (index = 0; index < G_N_ELEMENTS (wcs); index++)
                            if (wcs[index] == L' ' && (index == G_N_ELEMENTS (wcs) - 1 || wcs[index + 1] == 0))
                              {
                                wcs[index] = 0;
                                break;
                              }
                          k = -1;
                        }
                      else if (k == 0)
                        {
                          /* This message won't be entirely correct for chained deadkeys, as there
                           * will be other deadkeys between dead_key->key.vk and vk
                           */
                          g_warning ("Keyboard layout (group %d) is abnormal, produces no output for dead key %d @ %u + %d @ %u\n",
                                     group, dead_key->key.vk, dead_key->key.level, vk, level);
                          reset_after_dead (key_state, hkls[group]);
                          continue;
                        }
                      else if (k == -1)
                        {
                          /* Also abnormal, but less clearly */
                          reset_after_dead (key_state, hkls[group]);
                          continue;
                        }
                      else
                        {
                          /* Undocumented */
                          g_warning ("ToUnicodeEx() returned %d for keycode %u in group %u at level %u",
                                     k, vk, group, level);
                          reset_after_dead (key_state, hkls[group]);
                          continue;
                        }
                    }

                  if (k == -1 || k > 0)
                    {
                      GError *error = NULL;
                      ucs4 = g_utf16_to_ucs4 (wcs, k, NULL, &ucs4_chars, &error);

                      if (ucs4 == NULL)
                        {
                          g_warning ("Failed to convert %d UTF-16 code points to UCS-4: %s",
                                     k, error->message);
                          g_clear_error (&error);
                        }
                    }

                  if ((k == -1 || k > 0) && ucs4 != NULL)
                    {
                      gsize ucs4_index;
                      GdkWin32KeyEntry *entry;
                      gsize ligature_length;

                      /* We store GDK keyvals, not unicode characters */
                      for (ucs4_index = 0; ucs4_index < ucs4_chars; ucs4_index++)
                         ucs4[ucs4_index] = k == -1 ? (ucs4[ucs4_index] | GDK_KEY_UNIDEAD) : gdk_unicode_to_keyval (ucs4[ucs4_index]);

                      combo.key.vk = vk;
                      combo.key.level = level;
                      combo.result.gdk_keyval = ucs4[0];

                      if (ucs4_chars == 1)
                        {
                          combo.result.ligature = NULL;
                          g_free (ucs4);
                        }
                      else
                        {
                          combo.result.ligature = ucs4;
                        }

                      combo.chain = (k == -1) ? TRUE : FALSE;

                      /* Copy the keysym table entry into the combination struct,
                       * as we won't have access to the keymap when sorting.
                       */
                      entry = keyentry (keymap->keysym_tab, keymap->layout_handles->len, vk, group, level);
                      combo.entry.gdk_keyval = entry->gdk_keyval;
                      combo.entry.ligature = NULL;

                      if (entry->ligature != NULL)
                        {
                          for (ligature_length = 0; entry->ligature[ligature_length] != 0; ligature_length++)
                            ;
                          combo.entry.ligature = g_new0 (guint32, ligature_length + 1);
                          memcpy (combo.entry.ligature, entry->ligature, ligature_length * sizeof (guint32));
                        }

                      g_array_append_val (dead_key->combinations, combo);
                    }

                  if (k == -1 &&
                      ucs4 != NULL &&
                      !linear_find_dead_key_by_keyval (options->dead_keys, combo.result.gdk_keyval, &existing_deadkey_i))
                    {
                      /* Dead key chaining */
                      chained_dead_key.key.vk = vk;
                      chained_dead_key.key.level = level;
                      chained_dead_key.entry.gdk_keyval = combo.result.gdk_keyval;
                      chained_dead_key.entry.ligature = NULL;

                      if (ucs4_chars > 1)
                        {
                          chained_dead_key.entry.ligature = g_new0 (guint32, ucs4_chars + 1);
                          memcpy (chained_dead_key.entry.ligature, combo.result.ligature, ucs4_chars * sizeof (guint32));
                        }

                      chained_dead_key.combinations = g_array_new (FALSE, FALSE, sizeof (GdkWin32CombinationNode));
                      g_array_set_clear_func (chained_dead_key.combinations, (GDestroyNotify) gdk_win32_combination_node_clear);
                      chained_dead_key.parent = dead_key;
                      chained_dead_key.combo_index = dead_key->combinations->len - 1;
                      g_array_append_val (options->dead_keys, chained_dead_key);
                    }

                  if (k < -1)
                    {
                      g_warning ("ToUnicodeEx() returned %d for keycode %u in group %u at level %u",
                                 k, vk, group, level);
                    }

                  reset_after_dead (key_state, hkls[group]);
                }
            }

          g_list_free (stack);
        }

      g_array_sort (options->dead_keys, (GCompareFunc) sort_dead_keys_by_gdk_keyval);

      for (i = 0; i < options->dead_keys->len; i++)
        {
          GdkWin32DeadKeyNode *dead_key;

          dead_key = &g_array_index (options->dead_keys, GdkWin32DeadKeyNode, i);

          dead_key->parent = NULL;
          dead_key->combo_index = 0;
          g_array_sort (dead_key->combinations, (GCompareFunc) sort_combinations_by_gdk_keyval);
        }
    }

  GDK_NOTE (EVENTS, print_keysym_tab (keymap));

  check_that_active_layout_is_in_sync (keymap);
  current_serial = _gdk_keymap_serial;
}

/* Because this function can be called recursively,
 * it's convenient to split compose_buffer into head
 * (compose_buffer0) and tail (compose_buffer1).
 * This is based on the assumption that dead keys
 * produce spacing characters that occupy only one
 * codepoint.
 */
static GdkWin32KeymapMatch
check_compose_internal (GdkWin32Keymap *keymap,
                        guint32         compose_buffer0,
                        guint32        *compose_buffer1,
                        gsize           compose_buffer1_len,
                        guint32        *output,
                        gsize          *output_len,
                        gsize           depth)
{
  guint8 active_group;
  gsize deadkey_i, node_i;
  GdkWin32DeadKeyNode *dead_key;
  GdkWin32KeyGroupOptions *options;
  GdkWin32CombinationNode *node;
  gsize output_size;
  gsize output_i;

  g_return_val_if_fail (output != NULL && output_len != NULL, GDK_WIN32_KEYMAP_MATCH_NONE);

  output_size = *output_len;

  active_group = _gdk_win32_keymap_get_active_group (keymap);
  options = &g_array_index (keymap->options, GdkWin32KeyGroupOptions, active_group);

  /* No dead keys produce this keyval => no matches */
  if (!find_dead_key_by_keyval (options->dead_keys, compose_buffer0, &deadkey_i))
    return GDK_WIN32_KEYMAP_MATCH_NONE;

  /* We might have mulitple nodes for the same dead key, deadkey finder will
   * find one of them, but not necessarily the first one. Step back until
   * we find the earliest node with the same keycode.
   */
  while (deadkey_i > 0)
    {
      dead_key = &g_array_index (options->dead_keys, GdkWin32DeadKeyNode, deadkey_i - 1);

      if (dead_key->entry.gdk_keyval != compose_buffer0)
        break;

      deadkey_i--;
    }

  if ((compose_buffer0 & GDK_KEY_UNIDEAD) != GDK_KEY_UNIDEAD)
    return GDK_WIN32_KEYMAP_MATCH_NONE;

  /* Can't combine a buffer consisting of one keyval */
  if (compose_buffer1_len == 0)
    return GDK_WIN32_KEYMAP_MATCH_INCOMPLETE;

  dead_key = &g_array_index (options->dead_keys, GdkWin32DeadKeyNode, deadkey_i);

  /* Look for a valid combination with the second keyval */
  if (!find_combination_by_keyval (keymap, dead_key->combinations, compose_buffer1[0], &node_i))
    {
      /* Found no match, copy the compose buffer into the
       * output buffer verbatim.
       */
      if (output_size > 0)
        output[0] = compose_buffer0;

      for (output_i = 1;
           output_i < output_size;
           output_i++)
        output[output_i] = compose_buffer1[output_i - 1];

      *output_len = output_i;

      return GDK_WIN32_KEYMAP_MATCH_PARTIAL;
    }

  while (node_i > 0)
    {
      node = &g_array_index (dead_key->combinations, GdkWin32CombinationNode, node_i - 1);

      if (node->entry.gdk_keyval != compose_buffer1[0])
        break;

      node_i--;
    }

  node = &g_array_index (dead_key->combinations, GdkWin32CombinationNode, node_i);

  /* Found a matching combination.
   * If it's chained, we call this function recursively with different
   * compose buffer (the newly-produced substitution,
   * plus the rest of the buffer contents outside of the pair
   * that we just processed).
   */
  if (node->chain)
    {
      GdkWin32KeymapMatch sub;

      sub = check_compose_internal (keymap,
                                    node->result.gdk_keyval,
                                    &compose_buffer1[1],
                                    compose_buffer1_len - 1,
                                    output, output_len,
                                    depth + 1);
      if (sub == GDK_WIN32_KEYMAP_MATCH_EXACT ||
          sub == GDK_WIN32_KEYMAP_MATCH_PARTIAL ||
          sub == GDK_WIN32_KEYMAP_MATCH_INCOMPLETE)
        {
          /* Recursive invocation already filled the output buffer */
          return sub;
        }
    }

  if (output_size > 0)
    output[0] = node->result.gdk_keyval;

  for (output_i = 1;
       output_i < output_size &&
       node->result.ligature &&
       node->result.ligature[output_i] != 0;
       output_i++)
    output[output_i] = node->result.ligature[output_i];

  *output_len = output_i;

  return GDK_WIN32_KEYMAP_MATCH_EXACT;
}

GdkWin32KeymapMatch
gdk_win32_keymap_check_compose (GdkWin32Keymap *keymap,
                                guint16        *compose_buffer,
                                gsize           compose_buffer_len,
                                guint16        *output,
                                gsize          *output_len)
{
  GdkWin32KeymapMatch result;
  guint32 *compose_buffer32;
  guint32 *output32;
  gsize i;

  /* Can't combine an empty buffer */
  if (compose_buffer_len == 0)
    return GDK_WIN32_KEYMAP_MATCH_NONE;

  compose_buffer32 = g_new0 (guint32, compose_buffer_len);
  output32 = g_new0 (guint32, *output_len);

  for (i = 0; i < compose_buffer_len; i++)
    compose_buffer32[i] = compose_buffer[i];

  result = check_compose_internal (keymap,
                                   compose_buffer32[0], &compose_buffer32[1], compose_buffer_len - 1,
                                   output32, output_len, 0);

  if (result == GDK_WIN32_KEYMAP_MATCH_EXACT ||
      result == GDK_WIN32_KEYMAP_MATCH_PARTIAL)
    for (i = 0; i < *output_len; i++)
      output[i] = (guint16) output32[i];

  g_free (output32);
  g_free (compose_buffer32);

  return result;
}

GdkWin32KeymapMatch
gdk_win32_keymap_check_compose32 (GdkWin32Keymap *keymap,
                                  guint32        *compose_buffer,
                                  gsize           compose_buffer_len,
                                  guint32        *output,
                                  gsize          *output_len)
{
  GdkWin32KeymapMatch result;

  /* Can't combine an empty buffer */
  if (compose_buffer_len == 0)
    return GDK_WIN32_KEYMAP_MATCH_NONE;

  result = check_compose_internal (keymap,
                                   compose_buffer[0], &compose_buffer[1], compose_buffer_len - 1,
                                   output, output_len, 0);

  return result;
}

guint8
_gdk_win32_keymap_get_rshift_scancode (GdkWin32Keymap *keymap)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).scancode_rshift;

  return 0;
}

void
_gdk_win32_keymap_set_active_layout (GdkWin32Keymap *keymap,
                                     HKL             hkl)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    {
      gint group;

      for (group = 0; group < keymap->layout_handles->len; group++)
        if (g_array_index (keymap->layout_handles, HKL, group) == hkl)
          keymap->active_layout = group;
    }
}

gboolean
_gdk_win32_keymap_has_altgr (GdkWin32Keymap *keymap)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).has_altgr;

  return FALSE;
}

guint8
_gdk_win32_keymap_get_active_group (GdkWin32Keymap *keymap)
{
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return keymap->active_layout;

  return 0;
}

GdkKeymap*
_gdk_win32_display_get_keymap (GdkDisplay *display)
{
  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_win32_keymap_get_type (), NULL);

  return default_keymap;
}

static PangoDirection
get_hkl_direction (HKL hkl)
{
  switch (PRIMARYLANGID (LOWORD ((DWORD) (gintptr) hkl)))
    {
    case LANG_HEBREW:
    case LANG_ARABIC:
#ifdef LANG_URDU
    case LANG_URDU:
#endif
    case LANG_FARSI:
      /* Others? */
      return PANGO_DIRECTION_RTL;

    default:
      return PANGO_DIRECTION_LTR;
    }
}

static PangoDirection
gdk_win32_keymap_get_direction (GdkKeymap *gdk_keymap)
{
  HKL active_hkl;
  GdkWin32Keymap *keymap;

  if (gdk_keymap == NULL || gdk_keymap != gdk_keymap_get_default ())
    keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  else
    keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (GDK_KEYMAP (keymap));

  if (keymap->layout_handles->len <= 0)
    active_hkl = GetKeyboardLayout (0);
  else
    active_hkl = g_array_index (keymap->layout_handles, HKL, keymap->active_layout);

  return get_hkl_direction (active_hkl);
}

static gboolean
gdk_win32_keymap_have_bidi_layouts (GdkKeymap *gdk_keymap)
{
  GdkWin32Keymap *keymap;
  gboolean        have_rtl = FALSE;
  gboolean        have_ltr = FALSE;
  gint            group;

  if (gdk_keymap == NULL || gdk_keymap != gdk_keymap_get_default ())
    keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  else
    keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (GDK_KEYMAP (keymap));

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      if (get_hkl_direction (g_array_index (keymap->layout_handles, HKL, group)) == PANGO_DIRECTION_RTL)
        have_rtl = TRUE;
      else
        have_ltr = TRUE;
    }

  return have_ltr && have_rtl;
}

static gboolean
gdk_win32_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_CAPITAL) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_NUMLOCK) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  (void) keymap;

  return ((GetKeyState (VK_SCROLL) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_entries_for_keyval (GdkKeymap     *gdk_keymap,
                                         guint          keyval,
                                         GdkKeymapKey **keys,
                                         gint          *n_keys)
{
  GArray *retval;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  /* Accept only the default keymap */
  if (gdk_keymap == NULL || gdk_keymap == gdk_keymap_get_default ())
    {
      gint vk;
      GdkWin32Keymap *keymap;

      if (gdk_keymap == NULL)
        keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
      else
        keymap = GDK_WIN32_KEYMAP (gdk_keymap);

      update_keymap (gdk_keymap);

      for (vk = 0; vk < KEY_STATE_SIZE; vk++)
        {
          gint group;

          for (group = 0; group < keymap->layout_handles->len; group++)
            {
              GdkWin32KeyLevelState    level;

              for (level = GDK_WIN32_LEVEL_NONE; level < GDK_WIN32_LEVEL_COUNT; level++)
                {
                  GdkWin32KeyEntry *entry;

                  entry = keyentry (keymap->keysym_tab, keymap->layout_handles->len, vk, group, level);

                  if (entry->gdk_keyval == keyval)
                    {
                      GdkKeymapKey key;

                      key.keycode = vk;
                      key.group = group;
                      key.level = level;
                      g_array_append_val (retval, key);
                    }
                }
            }
        }
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      guint i;

      g_print ("gdk_keymap_get_entries_for_keyval: %#.04x (%s):",
               keyval, gdk_keyval_name (keyval));
      for (i = 0; i < retval->len; i++)
        {
          GdkKeymapKey *entry = (GdkKeymapKey *) retval->data + i;
          g_print ("  %#.02x %d %d", entry->keycode, entry->group, entry->level);
        }
      g_print ("\n");
    }
#endif

  if (retval->len > 0)
    {
      *keys = (GdkKeymapKey*) retval->data;
      *n_keys = retval->len;
    }
  else
    {
      *keys = NULL;
      *n_keys = 0;
    }

  g_array_free (retval, retval->len > 0 ? FALSE : TRUE);

  return *n_keys > 0;
}

static gboolean
gdk_win32_keymap_get_entries_for_keycode (GdkKeymap     *gdk_keymap,
                                          guint          hardware_keycode,
                                          GdkKeymapKey **keys,
                                          guint        **keyvals,
                                          gint          *n_entries)
{
  GArray         *key_array;
  GArray         *keyval_array;
  gint            group;
  GdkWin32Keymap *keymap;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  if (hardware_keycode <= 0 ||
      hardware_keycode >= KEY_STATE_SIZE ||
      (keys == NULL && keyvals == NULL) ||
      (gdk_keymap != NULL && gdk_keymap != gdk_keymap_get_default ()))
    {
      /* Wrong keycode or NULL output arrays or wrong keymap */
      if (keys)
        *keys = NULL;
      if (keyvals)
        *keyvals = NULL;

      *n_entries = 0;
      return FALSE;
    }

  if (keys)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;

  if (keyvals)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  update_keymap (GDK_KEYMAP (keymap));

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      GdkWin32KeyLevelState    level;

      for (level = GDK_WIN32_LEVEL_NONE; level < GDK_WIN32_LEVEL_COUNT; level++)
        {
          if (key_array)
            {
              GdkKeymapKey key;

              key.keycode = hardware_keycode;
              key.group = group;
              key.level = level;
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            {
              GdkWin32KeyEntry *entry;
              entry = keyentry (keymap->keysym_tab, keymap->layout_handles->len, hardware_keycode, group, level);

              g_array_append_val (keyval_array, entry->gdk_keyval);
            }
        }
    }

  *n_entries = group * GDK_WIN32_LEVEL_COUNT;

  if ((key_array && key_array->len > 0) ||
      (keyval_array && keyval_array->len > 0))
    {
      if (keys)
        *keys = (GdkKeymapKey*) key_array->data;

      if (keyvals)
        *keyvals = (guint*) keyval_array->data;
    }
  else
    {
      if (keys)
        *keys = NULL;

      if (keyvals)
        *keyvals = NULL;
    }

  if (key_array)
    g_array_free (key_array, key_array->len > 0 ? FALSE : TRUE);
  if (keyval_array)
    g_array_free (keyval_array, keyval_array->len > 0 ? FALSE : TRUE);

  return *n_entries > 0;
}

static guint
gdk_win32_keymap_lookup_key (GdkKeymap          *gdk_keymap,
                             const GdkKeymapKey *key)
{
  GdkWin32KeyEntry *sym;
  GdkWin32Keymap *keymap;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), 0);
  g_return_val_if_fail (key != NULL, 0);

  /* Accept only the default keymap */
  if (gdk_keymap != NULL && gdk_keymap != gdk_keymap_get_default ())
    return 0;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  update_keymap (GDK_KEYMAP (keymap));

  if (key->keycode >= KEY_STATE_SIZE ||
      key->group < 0 || key->group >= keymap->layout_handles->len ||
      key->level < 0 || key->level >= GDK_WIN32_LEVEL_COUNT)
    return 0;

  sym = keyentry (keymap->keysym_tab, keymap->layout_handles->len, key->keycode, key->group, key->level);

  if (sym->gdk_keyval == GDK_KEY_VoidSymbol)
    return 0;
  else
    return sym->gdk_keyval;
}

static gboolean
gdk_win32_keymap_translate_keyboard_state (GdkKeymap       *gdk_keymap,
                                           guint            hardware_keycode,
                                           GdkModifierType  state,
                                           gint             group,
                                           guint           *keyval,
                                           gint            *effective_group,
                                           gint            *level,
                                           GdkModifierType *consumed_modifiers)
{
  GdkWin32Keymap *keymap;
  guint tmp_keyval;
  GdkWin32KeyEntry *keygroup;
  GdkWin32KeyLevelState shift_level;
  GdkModifierType modifiers = GDK_SHIFT_MASK | GDK_LOCK_MASK | GDK_MOD2_MASK;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), FALSE);

#if 0
  GDK_NOTE (EVENTS, g_print ("gdk_keymap_translate_keyboard_state: keycode=%#x state=%#x group=%d\n",
			     hardware_keycode, state, group));
#endif
  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  /* Accept only the default keymap */
  if (gdk_keymap != NULL && gdk_keymap != gdk_keymap_get_default ())
    return FALSE;

  if (hardware_keycode >= KEY_STATE_SIZE)
    return FALSE;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  update_keymap (GDK_KEYMAP (keymap));

  if (group < 0 || group >= keymap->layout_handles->len)
    return FALSE;

  keygroup = keyentry (keymap->keysym_tab, keymap->layout_handles->len, hardware_keycode, group, GDK_WIN32_LEVEL_NONE);

  if ((state & (GDK_SHIFT_MASK | GDK_LOCK_MASK)) == (GDK_SHIFT_MASK | GDK_LOCK_MASK))
    shift_level = GDK_WIN32_LEVEL_SHIFT_CAPSLOCK;
  else if (state & GDK_SHIFT_MASK)
    shift_level = GDK_WIN32_LEVEL_SHIFT;
  else if (state & GDK_LOCK_MASK)
    shift_level = GDK_WIN32_LEVEL_CAPSLOCK;
  else
    shift_level = GDK_WIN32_LEVEL_NONE;

  if (state & GDK_MOD2_MASK)
    {
      if (shift_level == GDK_WIN32_LEVEL_NONE)
        shift_level = GDK_WIN32_LEVEL_ALTGR;
      else if (shift_level == GDK_WIN32_LEVEL_SHIFT)
        shift_level = GDK_WIN32_LEVEL_SHIFT_ALTGR;
      else if (shift_level == GDK_WIN32_LEVEL_CAPSLOCK)
        shift_level = GDK_WIN32_LEVEL_CAPSLOCK_ALTGR;
      else
        shift_level = GDK_WIN32_LEVEL_SHIFT_CAPSLOCK_ALTGR;
    }

  /* Drop altgr, capslock and shift if there are no keysymbols on
   * the key for those.
   */
  if (keygroup[shift_level].gdk_keyval == GDK_KEY_VoidSymbol)
    {
      switch (shift_level)
        {
         case GDK_WIN32_LEVEL_NONE:
         case GDK_WIN32_LEVEL_ALTGR:
         case GDK_WIN32_LEVEL_SHIFT:
         case GDK_WIN32_LEVEL_CAPSLOCK:
           if (keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_NONE;
           break;
         case GDK_WIN32_LEVEL_SHIFT_CAPSLOCK:
           if (keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_CAPSLOCK;
           else if (keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_SHIFT;
           else if (keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_NONE;
           break;
         case GDK_WIN32_LEVEL_CAPSLOCK_ALTGR:
           if (keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_ALTGR;
           else if (keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_CAPSLOCK;
           else if (keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_NONE;
           break;
         case GDK_WIN32_LEVEL_SHIFT_ALTGR:
           if (keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_ALTGR;
           else if (keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_SHIFT;
           else if (keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_NONE;
           break;
         case GDK_WIN32_LEVEL_SHIFT_CAPSLOCK_ALTGR:
           if (keygroup[GDK_WIN32_LEVEL_CAPSLOCK_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_CAPSLOCK_ALTGR;
           else if (keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_SHIFT_ALTGR;
           else if (keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_ALTGR;
           else if (keygroup[GDK_WIN32_LEVEL_SHIFT_CAPSLOCK].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_SHIFT_CAPSLOCK;
           else if (keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_CAPSLOCK;
           else if (keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_SHIFT;
           else if (keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval != GDK_KEY_VoidSymbol)
             shift_level = GDK_WIN32_LEVEL_NONE;
           break;
         case GDK_WIN32_LEVEL_COUNT:
           g_assert_not_reached ();
        }
    }

  /* See whether the shift level actually mattered
   * to know what to put in consumed_modifiers
   */
  if ((keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval == keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval == keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_SHIFT_CAPSLOCK].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval == keygroup[GDK_WIN32_LEVEL_SHIFT_CAPSLOCK].gdk_keyval))
      modifiers &= ~GDK_SHIFT_MASK;

  if ((keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval == keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_CAPSLOCK_ALTGR].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval == keygroup[GDK_WIN32_LEVEL_CAPSLOCK_ALTGR].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_SHIFT_CAPSLOCK].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval == keygroup[GDK_WIN32_LEVEL_SHIFT_CAPSLOCK].gdk_keyval))
      modifiers &= ~GDK_LOCK_MASK;

  if ((keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_NONE].gdk_keyval == keygroup[GDK_WIN32_LEVEL_ALTGR].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_SHIFT].gdk_keyval == keygroup[GDK_WIN32_LEVEL_SHIFT_ALTGR].gdk_keyval) &&
      (keygroup[GDK_WIN32_LEVEL_CAPSLOCK_ALTGR].gdk_keyval == GDK_KEY_VoidSymbol ||
       keygroup[GDK_WIN32_LEVEL_CAPSLOCK].gdk_keyval == keygroup[GDK_WIN32_LEVEL_CAPSLOCK_ALTGR].gdk_keyval))
      modifiers &= ~GDK_MOD2_MASK;

  tmp_keyval = keygroup[shift_level].gdk_keyval;

  if (keyval)
    *keyval = tmp_keyval;

  if (effective_group)
    *effective_group = group;

  if (level)
    *level = shift_level;

  if (consumed_modifiers)
    *consumed_modifiers = modifiers;

#if 0
  GDK_NOTE (EVENTS, g_print ("... group=%d level=%d cmods=%#x keyval=%s\n",
			     group, shift_level, modifiers, gdk_keyval_name (tmp_keyval)));
#endif

  return tmp_keyval != GDK_KEY_VoidSymbol;
}

const guint32*
gdk_win32_keymap_fetch_ligature (GdkWin32Keymap *keymap,
                                 GdkEventKey    *event)
{
  gint effective_group, level;
  GdkWin32KeyEntry *entry;

  /* Convert event state into shift level (also group) */
  if (!gdk_win32_keymap_translate_keyboard_state (GDK_KEYMAP (keymap),
                                                  event->hardware_keycode,
                                                  event->state,
                                                  event->group,
                                                  NULL,
                                                  &effective_group, &level, NULL))
    return NULL;

  entry = keyentry (keymap->keysym_tab, keymap->layout_handles->len,
                    event->hardware_keycode, effective_group, level);

  return entry->ligature;
}

static void
gdk_win32_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
                                        GdkModifierType *state)
{
}

static gboolean
gdk_win32_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                        GdkModifierType *state)
{
  /* FIXME: Is this the right thing to do? */
  return TRUE;
}

static void
gdk_win32_keymap_class_init (GdkWin32KeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_win32_keymap_finalize;

  keymap_class->get_direction = gdk_win32_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_win32_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_win32_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_win32_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_win32_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_win32_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_win32_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_win32_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_win32_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_win32_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_win32_keymap_map_virtual_modifiers;
}
