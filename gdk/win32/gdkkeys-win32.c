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

/* See documentation for VkKeyScanExW:
   https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-vkkeyscanexw */
enum _Win32ModMask
  {
    WIN32_MOD_SHIFT     =  1,
    WIN32_MOD_CTRL      =  2,
    WIN32_MOD_ALT       =  4,
    WIN32_MOD_HANKAKU   =  8,
    WIN32_MOD_RESERVED1 = 16,
    WIN32_MOD_RESERVED2 = 32
  };

typedef enum _Win32ModMask Win32ModMask;

/* Levels are ordered by the number of bits set.
   I.e. masks with just one bit set come before masks with two bits set and so on. */
/* Associate bitmask -> level */
static const gint win32_mod_mask_to_level[] =
  {
    0x00, 0x01, 0x02, 0x07, 0x03, 0x08, 0x09, 0x16,
    0x04, 0x0a, 0x0b, 0x17, 0x0c, 0x18, 0x19, 0x2a,
    0x05, 0x0d, 0x0e, 0x1a, 0x0f, 0x1b, 0x1c, 0x2b,
    0x10, 0x1d, 0x1e, 0x2c, 0x1f, 0x2d, 0x2e, 0x39,
    0x06, 0x11, 0x12, 0x20, 0x13, 0x21, 0x22, 0x2f,
    0x14, 0x23, 0x24, 0x30, 0x25, 0x31, 0x32, 0x3a,
    0x15, 0x26, 0x27, 0x33, 0x28, 0x34, 0x35, 0x3b,
    0x29, 0x36, 0x37, 0x3c, 0x38, 0x3d, 0x3e, 0x3f
  };
/* Associate level -> bitmask */
static const gint win32_level_to_mod_mask[] =
  {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x03,
    0x05, 0x06, 0x09, 0x0a, 0x0c, 0x11, 0x12, 0x14,
    0x18, 0x21, 0x22, 0x24, 0x28, 0x30, 0x07, 0x0b,
    0x0d, 0x0e, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c,
    0x23, 0x25, 0x26, 0x29, 0x2a, 0x2c, 0x31, 0x32,
    0x34, 0x38, 0x0f, 0x17, 0x1b, 0x1d, 0x1e, 0x27,
    0x2b, 0x2d, 0x2e, 0x33, 0x35, 0x36, 0x39, 0x3a,
    0x3c, 0x1f, 0x2f, 0x37, 0x3b, 0x3d, 0x3e, 0x3f
  };

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

  /* Index of a handle in layout_handles,
   * at any point it should be the same handle as GetKeyboardLayout(0) returns,
   * but GDK caches it to avoid calling GetKeyboardLayout (0) every time.
   */
  guint8 active_layout;
};

G_DEFINE_TYPE (GdkWin32Keymap, gdk_win32_keymap, GDK_TYPE_KEYMAP)

guint _gdk_keymap_serial = 0;

static GdkKeymap *default_keymap = NULL;

#define KEY_STATE_SIZE 256

static void update_keymap (GdkKeymap *gdk_keymap);

static void
gdk_win32_keymap_init (GdkWin32Keymap *keymap)
{
  keymap->layout_handles = g_array_new (FALSE, FALSE, sizeof (HKL));
  keymap->active_layout = 0;
  update_keymap (GDK_KEYMAP (keymap));
}

static void
gdk_win32_keymap_finalize (GObject *object)
{
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (object);

  g_clear_pointer (&keymap->layout_handles, g_array_unref);

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
vk_and_mod_mask_to_gdk_keysym (HKL hkl, guint vk, Win32ModMask mod_mask)

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
  if (vk == VK_TAB && !(mod_mask & WIN32_MOD_SHIFT))
    return GDK_KEY_Tab;
  if (vk == VK_TAB &&  (mod_mask & WIN32_MOD_SHIFT))
    return GDK_KEY_ISO_Left_Tab;

  /* Generic special keys */
  switch (vk)
    {
      #define MAP(a_vk, a_gdk) case a_vk: return a_gdk;
      DEFINE_SPECIAL (MAP)
      #undef MAP
    }

  /* If the previous attempts failed, try converting to Unicode and back */

  /* Check for dead key */
  gunichar c = MapVirtualKeyExW (vk, MAPVK_VK_TO_CHAR, hkl);
  gboolean is_dead = c & 0x80000000;

  /* Note that we can't simply use MapVirtualKeyExW(vk, MAPVK_VK_TO_CHAR, hkl) because it
     always produces upper-case letters, but GDK expects lower case if shift is not pressed. */
  UINT scancode = MapVirtualKeyExW (vk, MAPVK_VK_TO_VSC, hkl);

  /* We need to query the existing keyboard state for NumLock, CapsLock etc. */
  BYTE saved_keystate[KEY_STATE_SIZE] = {0};
  GetKeyboardState (saved_keystate);

  BYTE keystate[KEY_STATE_SIZE] = {0};

  /* Copy over some keystates like numlock and capslock */
  static const guint mode_keys[] =
    {
      VK_CAPITAL,
      VK_KANA, VK_HANGUL, VK_JUNJA, VK_FINAL, VK_HANJA, VK_KANJI, /* Is this correct? */
      VK_NUMLOCK, VK_SCROLL
    };
  for (guint i = 0; i < sizeof (mode_keys) / sizeof (mode_keys[0]); ++i)
    keystate[i] = saved_keystate[i];
  
  /* Add own modifiers */
  if (mod_mask & WIN32_MOD_SHIFT)
    keystate[VK_SHIFT] = keystate[VK_LSHIFT] = 0x80;

  /* Ctrl normally shifts down by 64, i.e. Ctrl+A = 0x01. We don't want this, so we normally
     skip this modifier.
     However, AltGr is emulated by Ctrl+Alt, so if both are set, we DO want to set them. */
  if ((mod_mask & (WIN32_MOD_CTRL | WIN32_MOD_ALT)) == (WIN32_MOD_CTRL | WIN32_MOD_ALT))
    {
      keystate[VK_CONTROL] = keystate[VK_LCONTROL] = 0x80;
      keystate[VK_MENU] = keystate[VK_LMENU] = 0x80;
    }

  WCHAR utf16_buf[64] = {0};
  int   utf16_n = 0;
  utf16_n = ToUnicodeEx (vk, scancode, keystate, utf16_buf, sizeof (utf16_buf) / sizeof (utf16_buf[0]),
                         0, hkl);

  /* For dead keys, double-press. Don't know if this always works? */
  if (is_dead)
    {
      GetKeyboardState (keystate);
      utf16_n = ToUnicodeEx (vk, scancode, keystate, utf16_buf, sizeof (utf16_buf)  / sizeof (utf16_buf[0]),
			     0, hkl);
    }

  /* ToUnicodeEx clobbers the global keystate so we have to reset it */
  SetKeyboardState(saved_keystate);

  c = utf16_buf[0];

  /* Fix up dead keys */
  if (utf16_n == -1)
    {
      guint sym = gdk_unicode_to_keyval (c);
      switch (sym)
	{
	  #define MAP(a_nondead, a_dead) case a_nondead: return a_dead;
	  DEFINE_DEAD (MAP)
	  #undef MAP
	}
    }

  /* Todo: Handle case where more than one wchar is returned */

  return gdk_unicode_to_keyval(c);
}

static void
gdk_keysym_to_vk_and_mod_mask (HKL hkl, guint sym, guint *vk, Win32ModMask *mod_mask)
{
  *mod_mask = 0;

  /* Special cases */
  if (sym == GDK_KEY_Tab)
    {
      *vk = VK_TAB;
      return;
    }
  if (sym == GDK_KEY_ISO_Left_Tab)
    {
      *vk = VK_TAB;
      *mod_mask = WIN32_MOD_SHIFT;
      return;
    }

  /* Generic non-printable keys */
  switch (sym)
    {
      #define MAP(a_vk, a_gdk) case a_gdk: *vk = a_vk; return;
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
  SHORT vk_and_mods = VkKeyScanExW (c, hkl);
  if (vk_and_mods == 0xFFFF)
    {
      *vk = *mod_mask = 0;
    }
  else
    {
      *vk = vk_and_mods & 0xFF;
      *mod_mask = (vk_and_mods >> 8) & 0xFF;
    }
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

static void
update_keymap (GdkKeymap *gdk_keymap)
{
  int             hkls_len;
  static int      hkls_size = 0;
  static HKL     *hkls = NULL;
  gboolean        no_list;
  static guint    current_serial = 0;
  gint            i;
  GdkWin32Keymap *keymap = GDK_WIN32_KEYMAP (gdk_keymap);

  if (hkls_len > 0 && current_serial == _gdk_keymap_serial)
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

  g_array_set_size (keymap->layout_handles, hkls_len);

  for (i = 0; i < hkls_len; i++)
    {
      g_array_index (keymap->layout_handles, HKL, i) = hkls[i];

      if (hkls[i] == _gdk_input_locale)
        keymap->active_layout = i;
    }

  check_that_active_layout_is_in_sync (keymap);
  current_serial = _gdk_keymap_serial;
}

guint8
_gdk_win32_keymap_get_rshift_scancode (GdkWin32Keymap *keymap)
{
#if 0
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).scancode_rshift;

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
  /* For now we just return FALSE, since it doesn't really matter because AltGr is just Ctrl + Alt.
     This implies that we will never get a GDK_MOD2_MASK, instead we will just get
     GDK_CONTROL_MASK | GDK_MOD1_MASK.
     But I don't think we have any clean way of distinguishing those cases under Windows anyway. */
#if 0
  if (keymap != NULL &&
      keymap->layout_handles->len > 0)
    return g_array_index (keymap->options, GdkWin32KeyGroupOptions, keymap->active_layout).has_altgr;
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

      gint group;

      for (group = 0; group < keymap->layout_handles->len; group++)
        {
          guint vk = 0;
          Win32ModMask hotkey_mask = WIN32_MOD_CTRL | WIN32_MOD_ALT | WIN32_MOD_SHIFT;
          /* base_mask is essentially the set of modifiers that were "consumed"
             in creating the keyval */
          Win32ModMask base_mask = 0;
          HKL hkl = g_array_index (keymap->layout_handles, HKL, group);
          gdk_keysym_to_vk_and_mod_mask (hkl, keyval, &vk, &base_mask);
          if (vk != 0)
            {
              /* Combine base mask with additional modifiers. If those modifiers don't affect the
	         keyval, add them. 

                 NOTE: This is SLOW, because the GTK way of handling shortcuts is super-inefficient:

                 GTK wants *every* modifier+key combination that could produce a specific keyval.
                 The problem here is that this means the number increases exponentially with the 
		 number of modifiers. On Windows we have 6 potential modifiers, but most of them 
		 don't affect the resulting key in 99 % of all cases.

                 So we get ~64 redundant entries for just one key.
                 
                 Unfortunately, the call to vk_and_mod_mask_to_gdk_keysym() isn't exactly fast either
		 since it involves manipulating the global 256 bytes long keyboard state due to the
		 way the Windows API works. So it really makes a noticeable difference whether we
		 do 1 call or 64 calls.
                 
                 Thus we hand-optimize here to try as few combinations as possible. We assume that
		 [Ctrl] and [Alt] don't affect the keysym, however [Ctrl+Alt] might, since it emulates
		 AltGr. We ignore HANKAKU, RESERVED1 and RESERVED2 because they don't seem to affect
		 shortcuts on Windows (tested in Notepad). Besides, GTK can't handle them anyway. */
              #if 1
              for (int shift_yes = 0; shift_yes <= 1; ++shift_yes)
                {
                  for (int altgr_yes = 0; altgr_yes <= 1; ++altgr_yes)
		    {
		      Win32ModMask mod_mask = 0;
		      if (shift_yes)
			mod_mask |= WIN32_MOD_SHIFT;
		      if (altgr_yes)
			mod_mask |= WIN32_MOD_CTRL | WIN32_MOD_ALT;
		      guint kv = vk_and_mod_mask_to_gdk_keysym (hkl, vk, mod_mask);
		      if (kv != keyval)
			continue;
                    
		      GdkKeymapKey key;
		      key.keycode = vk;
		      key.group = group;
		      key.level = win32_mod_mask_to_level[mod_mask];
		      g_array_append_val (retval, key);
                    
		      /* Now add the redundant modifiers Ctrl and Alt */
		      if (!altgr_yes)
			{
			  key.level = win32_mod_mask_to_level[mod_mask | WIN32_MOD_CTRL];
			  g_array_append_val (retval, key);
			  key.level = win32_mod_mask_to_level[mod_mask | WIN32_MOD_ALT];
			  g_array_append_val (retval, key);
			}
		    }
                }
              #else
              /* Slower, more general code, may be useful for debugging */
              for (gint i = 0; i < 64; ++i)
                {
                  Win32ModMask extra_mask = win32_level_to_mod_mask[i];
                  /* We are only interested in combinations of [Ctrl, Alt, Shift].
                     Reject everything else for performance reasons. */
                  if (extra_mask & ~hotkey_mask)
                    continue;
                  Win32ModMask mod_mask = base_mask | extra_mask;
                  /* Ignore combinations which are subsets of the base_mask, so we
                     don't add the same combination multiple times. */
                  if (mod_mask != 0 && (mod_mask | base_mask) == base_mask)
                    continue;
                  guint kv = vk_and_mod_mask_to_gdk_keysym (hkl, vk, mod_mask);
                  if (kv == keyval)
                    {
                      GdkKeymapKey key;
                      key.keycode = vk;
                      key.group = group;
                      key.level = win32_mod_mask_to_level[mod_mask];
                      g_array_append_val (retval, key);
                    }
                }

                #endif
           }
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

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

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

  guint vk = hardware_keycode;

  for (group = 0; group < keymap->layout_handles->len; group++)
    {
        HKL hkl = g_array_index (keymap->layout_handles, HKL, group);
        for (int i = 0; i < 64; ++i)
          {
            Win32ModMask mod_mask = win32_level_to_mod_mask[i];
            guint keyval = vk_and_mod_mask_to_gdk_keysym (hkl, vk, mod_mask);
            Win32ModMask consumed_mask = 0;
            guint tmp_vk = 0;
            gdk_keysym_to_vk_and_mod_mask (hkl, keyval, &tmp_vk, &consumed_mask);
            if (tmp_vk == 0 || tmp_vk != vk)
              continue;
            /* Only add canonical representation. Is this correct? */
            if (mod_mask == consumed_mask) {
              GdkKeymapKey key;
              key.keycode = hardware_keycode;
              key.group = group;
              key.level = i;
              g_array_append_val (key_array, key);

              if (keyval_array)
                g_array_append_val (keyval_array, keyval);
            }
          }
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
  guint sym;
  GdkWin32Keymap *keymap;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), 0);
  g_return_val_if_fail (key != NULL, 0);

  /* Accept only the default keymap */
  if (gdk_keymap != NULL && gdk_keymap != gdk_keymap_get_default ())
    return 0;

  keymap = GDK_WIN32_KEYMAP (gdk_keymap_get_default ());
  update_keymap (GDK_KEYMAP (keymap));

  if (key->group < 0 || key->group >= keymap->layout_handles->len)
    return 0;
  if (key->level < 0 || key->level >= 64)
    return 0;
  HKL hkl = g_array_index (keymap->layout_handles, HKL, key->group);
  Win32ModMask mod_mask = win32_level_to_mod_mask[key->level];
  sym = vk_and_mod_mask_to_gdk_keysym (hkl, key->keycode, mod_mask);

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
  GdkModifierType tmp_consumed_modifiers;

  HKL active_hkl;
  guint vk;
  Win32ModMask mod_mask;

  g_return_val_if_fail (gdk_keymap == NULL || GDK_IS_KEYMAP (gdk_keymap), FALSE);

  keymap = GDK_WIN32_KEYMAP (gdk_keymap);
  update_keymap (keymap);

  active_hkl = g_array_index (keymap->layout_handles, HKL, keymap->active_layout);

  vk = hardware_keycode;
  
  mod_mask = 0;
  if (state & GDK_SHIFT_MASK && vk != VK_SHIFT && vk != VK_LSHIFT && vk != VK_RSHIFT)
    mod_mask |= WIN32_MOD_SHIFT;
  if (state & GDK_CONTROL_MASK && vk != VK_CONTROL && vk != VK_LCONTROL && vk != VK_RCONTROL)
    mod_mask |= WIN32_MOD_CTRL;
  if (state & GDK_MOD1_MASK && vk != VK_MENU && vk != VK_LMENU && vk != VK_RMENU)
    mod_mask |= WIN32_MOD_ALT;
  /* AltGr is emulated as Ctrl+Alt here */
  if (state & GDK_MOD2_MASK && vk != VK_RMENU)
    mod_mask |= WIN32_MOD_CTRL | WIN32_MOD_ALT;
  /* TODO: How to handle other modifiers? Should we handle those at all? */

  tmp_keyval = vk_and_mod_mask_to_gdk_keysym (active_hkl, vk, mod_mask);
  tmp_effective_group = group;
  tmp_level = win32_mod_mask_to_level[mod_mask];
  tmp_consumed_modifiers = 0;

  /* Determine consumed modifiers */
  guint tmp_vk = 0;
  gdk_keysym_to_vk_and_mod_mask (active_hkl, tmp_keyval, &tmp_vk, &tmp_consumed_modifiers);
  
  if (keyval)
    *keyval = tmp_keyval;
  if (effective_group)
    *effective_group = tmp_effective_group;
  if (level)
    *level = tmp_level;
  if (consumed_modifiers)
    *consumed_modifiers = tmp_consumed_modifiers;

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
