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
 * Modified by the GTK+ Team and others 1997-2019.  See the AUTHORS
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

/* For lookup table VK -> chars */
typedef struct 
{
  int table;
  int index;
} TableAndIndex;

/* For reverse lookup char -> VKs */
typedef struct
{
  BYTE mod_bits;
  BYTE vk;
  /* Index of next KeyEntry. -1 if there is no next entry. */
  int  next;
} KeyEntry;

typedef struct _KBDTABLES *PKBDTABLES;

typedef struct
{
  HKL            handle;
  /* Keyboard layout identifier */
  char           name[KL_NAMELENGTH];
  /* Path of the layout DLL */
  char          *file;
  /* Handle of the layout DLL */
  HINSTANCE      lib;
  /* The actual conversion tables provided by the layout DLL */
  PKBDTABLES     tables;
  /* VK -> chars lookup table so we don't have to do a linear scan 
   * every time we look up a key. */
  TableAndIndex  vk_lookup_table[256];
  /* List of entries for reverse (char ->VKs) lookup. */
  GArray        *key_entries;
  /* Reverse lookup table (char -> VKs). Key: Unichar. Value: int.
   * The value is used to index into the key_entries array. The key_entries
   * array can contain multiple consecutive entries for a given char.
   * The end of the list for the char is marked by a key entry that has
   * mod_bits and vk set to 0xFF. */
  GHashTable    *reverse_lookup_table;
  /* Map level to modbits */
  BYTE           level_to_modbits[256];
  /* Max Number of levels */
  BYTE           max_level;
  /* Maximum possible value of a modbits bitset. */
  BYTE           max_modbit_value;
} KeyboardLayoutInfo;

/* Some keyboard driver constants
 * Adapted from ReactOS's kbd.h:
 * See https://github.com/reactos/reactos/blob/master/sdk/include/ndk/kbd.h
 */

/* Modifier bits */
#define KBDBASE        0x00
#define KBDSHIFT       0x01
#define KBDCTRL        0x02
#define KBDALT         0x04
#define KBDKANA        0x08
#define KBDROYA        0x10
#define KBDLOYA        0x20
#define KBDGRPSELTAP   0x80

#define KBDALTGR (KBDCTRL| KBDALT)

/* */
#define SHFT_INVALID 0x0F

/* Char table constants */
#define WCH_NONE 0xF000
#define WCH_DEAD 0xF001
#define WCH_LGTR 0xF002

/* Char table flags */
#define CAPLOK      0x01
#define SGCAPS      0x02
#define CAPLOKALTGR 0x04
#define KANALOK     0x08
#define GRPSELTAP   0x80

/* Internal function table */

/* Unfortunately, Microsoft decided to declare all the pointers in their
 * keyboard layout structures as `__ptr64`, which leads to two problems:
 *
 *   (1) GCC does not support `__ptr64`
 *   (2) We need two versions of the same code because we don't know at
 *       compile time how wide the pointer will be, since the width depends
 *       on the platform where the binary is executed. (Note that a 32 bit
 *       binary can be executed on a 32 bit OS or on a 64 bit OS)
 *
 * To abstract all this away, we use macros to define variants for both 32 Bit
 * and 64 Bit and then utilize the `kbdfuncs` vtable to call the correct
 * functions at runtime.
 *
 * See gdkwin32kbd-private.h for more information.
 */

typedef struct
{
  gboolean  (*load_layout_dll)      (const char         *dll,
                                     KeyboardLayoutInfo *info);
  void      (*init_vk_lookup_table) (KeyboardLayoutInfo *info);
  BYTE      (*keystate_to_modbits)  (KeyboardLayoutInfo *info,
                                     const BYTE          keystate[256]);
  BYTE      (*modbits_to_level)     (KeyboardLayoutInfo *info,
                                     BYTE                modbits);
  WCHAR     (*vk_to_char_fuzzy)     (KeyboardLayoutInfo *info,
                                     const BYTE          keystate[256],
                                     BYTE                extra_mod_bits,
                                     BYTE               *consumed_mod_bits,
                                     gboolean           *is_dead,
                                     BYTE                vk);
} KbdFuncs;

static KbdFuncs *kbdfuncs = NULL;

/* Code for 64bit Windows or WOW64 */
#define NATIVE_PTR_SIZE 8
#include "gdkwin32kbd-private.h"
#undef NATIVE_PTR_SIZE

/* Code for 32bit Windows */
#define NATIVE_PTR_SIZE 4
#include "gdkwin32kbd-private.h"
#undef NATIVE_PTR_SIZE

/* Helper functions for loading the correct code variant (32 bit or 64 bit) */

static BOOL
is_wow64 (void)
{
  BOOL bIsWow64 = FALSE;
  typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
  LPFN_ISWOW64PROCESS fnIsWow64Process;
  fnIsWow64Process = (LPFN_ISWOW64PROCESS)
                     GetProcAddress (GetModuleHandle (TEXT("kernel32")),
				     "IsWow64Process");

  if (fnIsWow64Process != NULL)
    fnIsWow64Process (GetCurrentProcess (), &bIsWow64);
  return bIsWow64;
}

static void
init_kbdfuncs (void)
{
  if (kbdfuncs != NULL)
      return;
  BOOL use_8byte_pointer = (sizeof (void*) == 8) || is_wow64 ();
  kbdfuncs = use_8byte_pointer ? &kbdfuncs8 : &kbdfuncs4;
}

/* Convenience wrapper functions around the above vtable */

static BOOL
load_layout_dll (const char         *dll,
                 KeyboardLayoutInfo *info)
{
  init_kbdfuncs ();
  return kbdfuncs->load_layout_dll (dll, info);
}

static void
init_vk_lookup_table (KeyboardLayoutInfo *info)
{
  init_kbdfuncs ();
  kbdfuncs->init_vk_lookup_table (info);
}

static BYTE
keystate_to_modbits (KeyboardLayoutInfo *info,
                     const BYTE          keystate[256])
{
  init_kbdfuncs ();
  return kbdfuncs->keystate_to_modbits (info, keystate);
}

static BYTE
modbits_to_level (KeyboardLayoutInfo *info,
                  BYTE                modbits)
{
  init_kbdfuncs ();
  return kbdfuncs->modbits_to_level (info, modbits);
}

static WCHAR
vk_to_char_fuzzy (KeyboardLayoutInfo *info,
                  const BYTE          keystate[256],
                  BYTE                extra_mod_bits,
                  BYTE               *consumed_mod_bits,
                  gboolean           *is_dead,
                  BYTE                vk)
{
  init_kbdfuncs ();
  return kbdfuncs->vk_to_char_fuzzy (info, keystate, extra_mod_bits,
                                     consumed_mod_bits, is_dead, vk);
}

/* Get the file path of the keyboard layout dll */

static char*
get_keyboard_layout_file (const char *layout_name)
{
  HKEY   hkey          = 0;
  DWORD  var_type      = REG_SZ;
  char  *result        = NULL;
  DWORD  file_name_len = 0;
  int    dir_len       = 0;
  int    buf_len       = 0;

  static const char prefix[] = "SYSTEM\\CurrentControlSet\\Control\\"
                               "Keyboard Layouts\\";
  char kbdKeyPath[sizeof (prefix) + KL_NAMELENGTH];
  snprintf (kbdKeyPath, sizeof (prefix) + KL_NAMELENGTH, "%s%s", prefix,
            layout_name);

  if (RegOpenKeyExA (HKEY_LOCAL_MACHINE, (LPCSTR) kbdKeyPath, 0,
                     KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
    goto fail1;

  /* Get sizes */
  if (RegQueryValueExA (hkey, "Layout File", 0, &var_type, 0,
			&file_name_len) != ERROR_SUCCESS)
    goto fail2;

  dir_len = GetSystemDirectoryA (0, 0); /* includes \0 */
  if (dir_len == 0)
    goto fail2;

  /* Allocate buffer */
  buf_len = dir_len + (int) strlen ("\\") + file_name_len;
  result = (char*) g_malloc (buf_len);

  /* Append system directory. The -1 is because dir_len includes \0 */
  if (GetSystemDirectoryA (&result[0], dir_len) != dir_len - 1)
    goto fail3;

  /* Append directory separator */
  result[dir_len - 1] = '\\';

  /* Append file name */
  if (RegQueryValueExA (hkey, "Layout File", 0, &var_type,
			(LPBYTE) &result[dir_len], &file_name_len)
      != ERROR_SUCCESS)
    goto fail3;

  result[dir_len + file_name_len] = '\0';

  RegCloseKey (hkey);
  return result;

fail3:
  g_free (result);
fail2:
  RegCloseKey (hkey);
fail1:
  return NULL;
}

static void
clear_keyboard_layout_info (gpointer data)
{
  KeyboardLayoutInfo *layout_info = (KeyboardLayoutInfo*) data;
  g_free (layout_info->file);
  g_array_unref (layout_info->key_entries);
  g_hash_table_destroy (layout_info->reverse_lookup_table);
  FreeLibrary (layout_info->lib);
  memset (layout_info, 0, sizeof (KeyboardLayoutInfo));
}

/* GdkWin32Keymap */

struct _GdkWin32KeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkWin32Keymap
{
  GdkKeymap parent_instance;
  
  /* Array of HKL */
  GArray *layout_handles;
  /* Array of KeyboardLayoutInfo */
  GArray *layout_infos;

  /* Index of a handle in layout_handles,
   * at any point it should be the same handle as GetKeyboardLayout(0) returns,
   * but GDK caches it to avoid calling GetKeyboardLayout (0) every time.
   */
  guint8  active_layout;

  guint current_serial;
};

G_DEFINE_TYPE (GdkWin32Keymap, gdk_win32_keymap, GDK_TYPE_KEYMAP)

guint _gdk_keymap_serial = 0;
static GdkKeymap *default_keymap = NULL;

static void update_keymap (GdkKeymap *gdk_keymap);

static void
gdk_win32_keymap_init (GdkWin32Keymap *keymap)
{
  keymap->layout_infos = g_array_new (FALSE, TRUE,
                                      sizeof (KeyboardLayoutInfo));
  g_array_set_clear_func (keymap->layout_infos,
                          clear_keyboard_layout_info);

  keymap->layout_handles = g_array_new (FALSE, FALSE,
                                        sizeof (KeyboardLayoutInfo));

  keymap->active_layout = 0;
  update_keymap (GDK_KEYMAP (keymap));
}

static void
gdk_win32_keymap_finalize (GObject *object)
{
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (object);

  g_clear_pointer (&keymap->layout_handles, g_array_unref);
  g_clear_pointer (&keymap->layout_infos, g_array_unref);

  G_OBJECT_CLASS (gdk_win32_keymap_parent_class)->finalize (object);
}

#define DEFINE_SPECIAL(map)                 \
  map (VK_CANCEL,     GDK_KEY_Cancel)       \
  map (VK_BACK,       GDK_KEY_BackSpace)    \
  map (VK_CLEAR,      GDK_KEY_Clear)        \
  map (VK_RETURN,     GDK_KEY_Return)       \
  map (VK_LSHIFT,     GDK_KEY_Shift_L)      \
  map (VK_LCONTROL,   GDK_KEY_Control_L)    \
  map (VK_LMENU,      GDK_KEY_Alt_L)        \
  map (VK_PAUSE,      GDK_KEY_Pause)        \
  map (VK_ESCAPE,     GDK_KEY_Escape)       \
  map (VK_PRIOR,      GDK_KEY_Prior)        \
  map (VK_NEXT,       GDK_KEY_Next)         \
  map (VK_END,        GDK_KEY_End)          \
  map (VK_HOME,       GDK_KEY_Home)         \
  map (VK_LEFT,       GDK_KEY_Left)         \
  map (VK_UP,         GDK_KEY_Up)           \
  map (VK_RIGHT,      GDK_KEY_Right)        \
  map (VK_DOWN,       GDK_KEY_Down)         \
  map (VK_SELECT,     GDK_KEY_Select)       \
  map (VK_PRINT,      GDK_KEY_Print)        \
  map (VK_EXECUTE,    GDK_KEY_Execute)      \
  map (VK_INSERT,     GDK_KEY_Insert)       \
  map (VK_DELETE,     GDK_KEY_Delete)       \
  map (VK_HELP,       GDK_KEY_Help)         \
  map (VK_LWIN,       GDK_KEY_Meta_L)       \
  map (VK_RWIN,       GDK_KEY_Meta_R)       \
  map (VK_APPS,       GDK_KEY_Menu)         \
  map (VK_DECIMAL,    GDK_KEY_KP_Decimal)   \
  map (VK_MULTIPLY,   GDK_KEY_KP_Multiply)  \
  map (VK_ADD,        GDK_KEY_KP_Add)       \
  map (VK_SEPARATOR,  GDK_KEY_KP_Separator) \
  map (VK_SUBTRACT,   GDK_KEY_KP_Subtract)  \
  map (VK_DIVIDE,     GDK_KEY_KP_Divide)    \
  map (VK_NUMPAD0,    GDK_KEY_KP_0)         \
  map (VK_NUMPAD1,    GDK_KEY_KP_1)         \
  map (VK_NUMPAD2,    GDK_KEY_KP_2)         \
  map (VK_NUMPAD3,    GDK_KEY_KP_3)         \
  map (VK_NUMPAD4,    GDK_KEY_KP_4)         \
  map (VK_NUMPAD5,    GDK_KEY_KP_5)         \
  map (VK_NUMPAD6,    GDK_KEY_KP_6)         \
  map (VK_NUMPAD7,    GDK_KEY_KP_7)         \
  map (VK_NUMPAD8,    GDK_KEY_KP_8)         \
  map (VK_NUMPAD9,    GDK_KEY_KP_9)         \
  map (VK_F1,         GDK_KEY_F1)           \
  map (VK_F2,         GDK_KEY_F2)           \
  map (VK_F3,         GDK_KEY_F3)           \
  map (VK_F4,         GDK_KEY_F4)           \
  map (VK_F5,         GDK_KEY_F5)           \
  map (VK_F6,         GDK_KEY_F6)           \
  map (VK_F7,         GDK_KEY_F7)           \
  map (VK_F8,         GDK_KEY_F8)           \
  map (VK_F9,         GDK_KEY_F9)           \
  map (VK_F10,        GDK_KEY_F10)          \
  map (VK_F11,        GDK_KEY_F11)          \
  map (VK_F12,        GDK_KEY_F12)          \
  map (VK_F13,        GDK_KEY_F13)          \
  map (VK_F14,        GDK_KEY_F14)          \
  map (VK_F15,        GDK_KEY_F15)          \
  map (VK_F16,        GDK_KEY_F16)          \
  map (VK_F17,        GDK_KEY_F17)          \
  map (VK_F18,        GDK_KEY_F18)          \
  map (VK_F19,        GDK_KEY_F19)          \
  map (VK_F20,        GDK_KEY_F20)          \
  map (VK_F21,        GDK_KEY_F21)          \
  map (VK_F22,        GDK_KEY_F22)          \
  map (VK_F23,        GDK_KEY_F23)          \
  map (VK_F24,        GDK_KEY_F24)          \
  map (VK_NUMLOCK,    GDK_KEY_Num_Lock)     \
  map (VK_SCROLL,     GDK_KEY_Scroll_Lock)  \
  map (VK_RSHIFT,     GDK_KEY_Shift_R)      \
  map (VK_RCONTROL,   GDK_KEY_Control_R)    \
  map (VK_RMENU,      GDK_KEY_Alt_R)  


#define DEFINE_DEAD(map)                                                      \
  map ('"',                          /* 0x022 */ GDK_KEY_dead_diaeresis)      \
  map ('\'',                         /* 0x027 */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_asciicircum,          /* 0x05e */ GDK_KEY_dead_circumflex)     \
  map (GDK_KEY_grave,                /* 0x060 */ GDK_KEY_dead_grave)          \
  map (GDK_KEY_asciitilde,           /* 0x07e */ GDK_KEY_dead_tilde)          \
  map (GDK_KEY_diaeresis,            /* 0x0a8 */ GDK_KEY_dead_diaeresis)      \
  map (GDK_KEY_degree,               /* 0x0b0 */ GDK_KEY_dead_abovering)      \
  map (GDK_KEY_acute,                /* 0x0b4 */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_periodcentered,       /* 0x0b7 */ GDK_KEY_dead_abovedot)       \
  map (GDK_KEY_cedilla,              /* 0x0b8 */ GDK_KEY_dead_cedilla)        \
  map (GDK_KEY_breve,                /* 0x1a2 */ GDK_KEY_dead_breve)          \
  map (GDK_KEY_ogonek,               /* 0x1b2 */ GDK_KEY_dead_ogonek)         \
  map (GDK_KEY_caron,                /* 0x1b7 */ GDK_KEY_dead_caron)          \
  map (GDK_KEY_doubleacute,          /* 0x1bd */ GDK_KEY_dead_doubleacute)    \
  map (GDK_KEY_abovedot,             /* 0x1ff */ GDK_KEY_dead_abovedot)       \
  map (0x1000384,              /* Greek tonos */ GDK_KEY_dead_acute)          \
  map (GDK_KEY_Greek_accentdieresis, /* 0x7ae */ GDK_KEY_Greek_accentdieresis)


static guint
vk_and_mod_bits_to_gdk_keysym (KeyboardLayoutInfo *info,
                               guint               vk,
                               const BYTE          keystate[256],
			       BYTE                mod_bits,
                               BYTE               *consumed_mod_bits)

{
  gboolean is_dead = FALSE;
  gunichar c = vk_to_char_fuzzy (info, keystate, mod_bits, consumed_mod_bits,
                                 &is_dead, vk);

  if (!is_dead)
    {
      /* Special cases */
      if (vk == VK_SHIFT)
        return GDK_KEY_Shift_L;
      if (vk == VK_CONTROL)
        return GDK_KEY_Control_L;
      if (vk == VK_MENU)
        return GDK_KEY_Alt_L;
      if (vk == VK_SNAPSHOT)
        return GDK_KEY_Print;
      if (vk == VK_TAB && !(mod_bits & KBDSHIFT))
        return GDK_KEY_Tab;
      if (vk == VK_TAB &&  (mod_bits & KBDSHIFT))
        return GDK_KEY_ISO_Left_Tab;

      /* Generic special keys */
      switch (vk)
        {
          #define MAP(a_vk, a_gdk) case a_vk: return a_gdk;
          DEFINE_SPECIAL (MAP)
          #undef MAP
        }
    }

  if (c == WCH_NONE)
    {
      if (consumed_mod_bits)
        *consumed_mod_bits = 0;
      return GDK_KEY_VoidSymbol;
    }

  guint sym = gdk_unicode_to_keyval (c);

  if (is_dead)
    {
      switch (sym)
	{
	  #define MAP(a_nondead, a_dead) case a_nondead: return a_dead;
	  DEFINE_DEAD (MAP)
	  #undef MAP
	}
    }

  return sym;
}

static gint
gdk_keysym_to_key_entry_index (KeyboardLayoutInfo *info,
	                       guint               sym)
{
  /* Special cases */
  if (sym == GDK_KEY_Tab)
      return VK_TAB;
  if (sym == GDK_KEY_ISO_Left_Tab)
      return 256;

  /* Generic non-printable keys */
  switch (sym)
    {
      #define MAP(a_vk, a_gdk) case a_gdk: return a_vk;
      DEFINE_SPECIAL (MAP)
      #undef MAP
    }

  /* Fix up dead keys */
  #define MAP(a_nondead, a_dead) \
    if (sym == a_dead)           \
      sym = a_nondead;
  DEFINE_DEAD (MAP)
  #undef MAP

  /* Try converting to Unicode and back */
  gunichar c = gdk_keyval_to_unicode (sym);

  gint index = -1;
  if (g_hash_table_lookup_extended (info->reverse_lookup_table,
                                    GINT_TO_POINTER (c),
                                    NULL, (gpointer*) &index))
    {
      return index;
    }
  else
    {
      return -1;
    }
}

static GdkModifierType
mod_bits_to_gdk_mod_mask (BYTE mod_bits)
{
    GdkModifierType result = 0;
    if (mod_bits & KBDSHIFT)
      result |= GDK_SHIFT_MASK;
    if (mod_bits & KBDCTRL)
      result |= GDK_CONTROL_MASK;
    if (mod_bits & KBDALT)
      result |= GDK_MOD1_MASK;
    if ((mod_bits & KBDALTGR) == KBDALTGR)
      result |= GDK_MOD2_MASK;
    if (mod_bits & KBDKANA)
      result |= GDK_MOD3_MASK;
    if (mod_bits & KBDROYA)
      result |= GDK_MOD4_MASK;
    if (mod_bits & KBDLOYA)
      result |= GDK_MODIFIER_RESERVED_13_MASK;
    if (mod_bits & KBDGRPSELTAP)
      result |= GDK_MODIFIER_RESERVED_14_MASK;
    return result;
}

static BYTE
gdk_mod_mask_to_mod_bits (GdkModifierType mod_mask)
{
    BYTE result = 0;
    if (mod_mask & GDK_SHIFT_MASK)
      result |= KBDSHIFT;
    if (mod_mask & GDK_CONTROL_MASK)
      result |= KBDCTRL;
    if (mod_mask & GDK_MOD1_MASK)
      result |= KBDALT;
    if (mod_mask & GDK_MOD2_MASK)
      result |= KBDALTGR;
    if (mod_mask & GDK_MOD3_MASK)
      result |= KBDKANA;
    if (mod_mask & GDK_MOD4_MASK)
      result |= KBDROYA;
    if (mod_mask & GDK_MODIFIER_RESERVED_13_MASK)
      result |= KBDLOYA;
    if (mod_mask & GDK_MODIFIER_RESERVED_14_MASK)
      result |= KBDGRPSELTAP;
    return result;
}

static void
get_lock_state (BYTE lock_state[256])
{
  BYTE keystate[256] = {0};
  GetKeyboardState (keystate);

  /* Copy over some keystates like numlock and capslock */
  static const guint mode_keys[] =
    {
      VK_CAPITAL,
      VK_KANA, VK_HANGUL, VK_JUNJA, VK_FINAL, VK_HANJA, VK_KANJI, /* Is this correct? */
      VK_NUMLOCK, VK_SCROLL
    };
  for (guint i = 0; i < sizeof (mode_keys) / sizeof (mode_keys[0]); ++i)
    lock_state[mode_keys[i]] = keystate[mode_keys[i]] & 0x1;
}


/* keypad decimal mark depends on active keyboard layout
 * return current decimal mark as unicode character
 */
guint32
_gdk_win32_keymap_get_decimal_mark (GdkWin32Keymap *keymap)
{
  #if 0
  if (keymap != NULL &&
      keymap->layout_handles->len > 0 &&
      g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).decimal_mark)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).decimal_mark;

  return (guint32) '.';
  #else
  guint32 c = MapVirtualKeyW (VK_DECIMAL, MAPVK_VK_TO_CHAR);
  if (!c)
    c = (guint32) '.';
  return c;
  #endif
}

static void
update_keymap (GdkKeymap *gdk_keymap)
{
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  HKL  current_layout;
  BOOL changed = FALSE;
  int  n_layouts;

  if (keymap->current_serial == _gdk_keymap_serial &&
      keymap->layout_handles->len > 0)
    {
      return;
    }

  n_layouts = GetKeyboardLayoutList (0, 0);
  g_array_set_size (keymap->layout_handles, n_layouts);
  n_layouts = GetKeyboardLayoutList (n_layouts,
		                     &g_array_index(keymap->layout_handles,
				                    HKL, 0));

  g_array_set_size (keymap->layout_infos, n_layouts);

  current_layout = GetKeyboardLayout (0);

  for (int i = 0; i < n_layouts; ++i)
    {
      KeyboardLayoutInfo *info = &g_array_index(keymap->layout_infos,
		                                KeyboardLayoutInfo, i);
      HKL hkl = g_array_index(keymap->layout_handles, HKL, i);
      if (info->handle != hkl)
        {
          changed = TRUE;

	  /* Free old data */
	  clear_keyboard_layout_info (info);

	  /* Load new data */
          info->handle = hkl;
          ActivateKeyboardLayout (hkl, 0);
          GetKeyboardLayoutNameA (info->name);
          info->file = get_keyboard_layout_file (info->name);

          info->key_entries = g_array_new (FALSE, FALSE, sizeof (KeyEntry));

          info->reverse_lookup_table = g_hash_table_new (g_direct_hash,
			                                 g_direct_equal);
          load_layout_dll (info->file, info);
          init_vk_lookup_table (info);
        }
      if (info->handle == current_layout)
        keymap->active_layout = i;
    }
  if (changed)
      ActivateKeyboardLayout (current_layout, 0);

  for (int i = 0; i < n_layouts; ++i)
    {
      KeyboardLayoutInfo *info = &g_array_index(keymap->layout_infos,
                                                KeyboardLayoutInfo, i);
      printf ("%d: %p %s%s\n", i, info->handle, info->name,
              info->handle == current_layout?" (active)":"");
      printf ("%s\n", info->file);
    }  

  keymap->current_serial = _gdk_keymap_serial;
}

guint8
_gdk_win32_keymap_get_rshift_scancode (GdkWin32Keymap *keymap)
{
#if 0
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions,
                          keymap->active_layout).scancode_rshift;

  return 0;
#else
  return MapVirtualKey (VK_RSHIFT, MAPVK_VK_TO_VSC);
#endif
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
  /* For now we just return FALSE, since it doesn't really matter because AltGr
   * is just Ctrl + Alt. This implies that we will never get a GDK_MOD2_MASK,
   * instead we will just get GDK_CONTROL_MASK | GDK_MOD1_MASK. But I don't
   * think we have any clean way of distinguishing those cases under Windows
   * anyway. */
#if 0
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions,
                          keymap->active_layout).has_altgr;
#endif
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


GdkModifierType
_gdk_win32_keymap_get_mod_mask (GdkWin32Keymap *keymap)
{
  GdkModifierType result;
  KeyboardLayoutInfo *layout_info;
    
  update_keymap (keymap);

  layout_info = &g_array_index (keymap->layout_infos, KeyboardLayoutInfo,
                                keymap->active_layout);
    
  BYTE keystate[256] = {0};
  GetKeyboardState (keystate);
    
  BYTE mod_bits = keystate_to_modbits (layout_info, keystate);
  return mod_bits_to_gdk_mod_mask (mod_bits);
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
  
  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), PANGO_DIRECTION_LTR);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (GDK_KEYMAP (keymap));

  if (keymap->layout_handles->len <= 0)
    active_hkl = GetKeyboardLayout (0);
  else
    active_hkl = g_array_index (keymap->layout_handles, HKL,
                                keymap->active_layout);

  return get_hkl_direction (active_hkl);
}

static gboolean
gdk_win32_keymap_have_bidi_layouts (GdkKeymap *gdk_keymap)
{
  GdkWin32Keymap *keymap;
  gboolean        have_rtl = FALSE;
  gboolean        have_ltr = FALSE;
  gint            group;
  
  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (GDK_KEYMAP (keymap));

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      if (get_hkl_direction (g_array_index (keymap->layout_handles, HKL,
                             group)) == PANGO_DIRECTION_RTL)
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
  GdkWin32Keymap *keymap;

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);

  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  update_keymap (gdk_keymap);

  BYTE keystate[256] = {0};

  for (gint group = 0; group < keymap->layout_handles->len; group++)
    {
      KeyboardLayoutInfo *info = &g_array_index (keymap->layout_infos,
                                                 KeyboardLayoutInfo, group);
      gint entry_index = gdk_keysym_to_key_entry_index (info, keyval);
      while (entry_index >= 0)
        {
          KeyEntry *entry = &g_array_index (info->key_entries, KeyEntry,
                                            entry_index);

          BYTE base_modbits = entry->mod_bits;
          
          /* Add original key combination */
          GdkKeymapKey gdk_key = {0};
          gdk_key.keycode = entry->vk;
          gdk_key.level   = modbits_to_level (info, entry->mod_bits);
          gdk_key.group   = group;

          g_array_append_val (retval, gdk_key);

          /* Add combinations with modifiers that do not affect the translation */
          for (BYTE extra_modbits = 0;
               extra_modbits <= info->max_modbit_value;
               ++extra_modbits)
            {
              /* We are only interested in masks that actually add anything
               * and do not overlap the original mask. */
              if ((extra_modbits | base_modbits) == base_modbits ||
                  (extra_modbits & base_modbits) != 0)
                continue;

              BYTE modbits = base_modbits | extra_modbits;

              /* Check if the additional modifiers change the semantics.
               * If they do not, add them. */
              guint sym = vk_and_mod_bits_to_gdk_keysym (info, entry->vk,
                                                         keystate, modbits,
                                                         NULL);
              if (sym == keyval || sym == GDK_KEY_VoidSymbol)
                {
                  gdk_key.keycode = entry->vk;
                  gdk_key.level   = modbits_to_level (info, modbits);
                  gdk_key.group   = group;
                  g_array_append_val (retval, gdk_key);
                }
            }

          entry_index = entry->next;
        }
   }

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

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  if (keys != NULL)
    key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  else
    key_array = NULL;

  if (keyvals != NULL)
    keyval_array = g_array_new (FALSE, FALSE, sizeof (guint));
  else
    keyval_array = NULL;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);

  BYTE keystate[256] = {0};

  BYTE vk = hardware_keycode;

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
      KeyboardLayoutInfo *info = &g_array_index (keymap->layout_infos, KeyboardLayoutInfo, group);
      for (int level = 0; level <= info->max_level; ++level)
        {
          BYTE modbits = info->level_to_modbits[level];
          BYTE consumed_modbits = 0;
          guint keyval = vk_and_mod_bits_to_gdk_keysym (info, vk, keystate, modbits, &consumed_modbits);

          if (keyval == GDK_KEY_VoidSymbol || consumed_modbits != modbits)
            continue;

          GdkKeymapKey key = {0};
          key.keycode = vk;
          key.group = group;
          key.level = level;

          if (key_array)
            g_array_append_val (key_array, key);

          if (keyval_array)
            g_array_append_val (keyval_array, keyval);

          ++(*n_entries);
        }
    }
  
  if (keys != NULL)
    *keys = g_array_free (key_array, FALSE);
  else
    g_array_free (key_array, TRUE);

  if (keyvals != NULL)
    *keyvals = g_array_free (keyval_array, FALSE);
  else
    g_array_free (keyval_array, TRUE);

  return *n_entries > 0;
}

static guint
gdk_win32_keymap_lookup_key (GdkKeymap          *gdk_keymap,
                             const GdkKeymapKey *key)
{
  guint sym;
  GdkWin32Keymap *keymap;
  BYTE modbits;
  BYTE keystate[256] = {0};

  g_return_val_if_fail (GDK_IS_KEYMAP (gdk_keymap), 0);
  g_return_val_if_fail (key != NULL, 0);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);
  KeyboardLayoutInfo *info = &g_array_index (keymap->layout_infos, KeyboardLayoutInfo, key->group);

  if (key->group < 0 || key->group >= keymap->layout_handles->len)
    return 0;
  if (key->level < 0 || key->level > info->max_level)
    return 0;
  
  modbits = info->level_to_modbits[key->level];

  sym = vk_and_mod_bits_to_gdk_keysym (info, key->keycode, keystate, modbits, NULL);
  if (sym == GDK_KEY_VoidSymbol)
    return 0;
  else
    return sym;
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
  guint           tmp_keyval;
  gint            tmp_effective_group;
  gint            tmp_level;
  BYTE            consumed_mod_bits;

  KeyboardLayoutInfo *layout_info;
  guint vk;
  BYTE mod_bits;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap),
                        FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);
  
  layout_info = &g_array_index (keymap->layout_infos, KeyboardLayoutInfo,
                                keymap->active_layout);

  vk = hardware_keycode;
  mod_bits = gdk_mod_mask_to_mod_bits (state);

  if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT)
    mod_bits &= ~KBDSHIFT;
  if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL)
    mod_bits &= ~KBDCTRL;
  if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU)
    mod_bits &= ~KBDALT;
  if (vk == VK_RMENU)
    mod_bits &= ~KBDALTGR;

  /* We need to query the existing keyboard state for NumLock, CapsLock etc. */
  BYTE keystate[256] = {0};
  get_lock_state (keystate);

  tmp_keyval = vk_and_mod_bits_to_gdk_keysym (layout_info, vk, keystate, mod_bits,
                                              &consumed_mod_bits);
  tmp_effective_group = group;
  tmp_level = modbits_to_level (layout_info, consumed_mod_bits);

  /* Determine consumed modifiers */
  
  if (keyval)
    *keyval = tmp_keyval;
  if (effective_group)
    *effective_group = tmp_effective_group;
  if (level)
    *level = tmp_level;
  if (consumed_modifiers)
    *consumed_modifiers = mod_bits_to_gdk_mod_mask (consumed_mod_bits);

  return tmp_keyval != GDK_KEY_VoidSymbol;
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
