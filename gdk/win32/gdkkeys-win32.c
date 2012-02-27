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

#include "config.h"

struct _GdkWin32KeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkWin32Keymap
{
  GdkKeymap parent_instance;
};

G_DEFINE_TYPE (GdkWin32Keymap, gdk_win32_keymap, GDK_TYPE_KEYMAP)

static void
gdk_win32_keymap_init (GdkWin32Keymap *keymap)
{
}

guint _gdk_keymap_serial = 0;
gboolean _gdk_keyboard_has_altgr = FALSE;
guint _scancode_rshift = 0;

static GdkModifierType gdk_shift_modifiers = GDK_SHIFT_MASK;

static GdkKeymap *default_keymap = NULL;

static guint *keysym_tab = NULL;

#ifdef G_ENABLE_DEBUG
static void
print_keysym_tab (void)
{
  gint vk;
  
  g_print ("keymap:%s%s\n",
	   _gdk_keyboard_has_altgr ? " (uses AltGr)" : "",
	   (gdk_shift_modifiers & GDK_LOCK_MASK) ? " (has ShiftLock)" : "");
  for (vk = 0; vk < 256; vk++)
    {
      gint state;
      
      g_print ("%#.02x: ", vk);
      for (state = 0; state < 4; state++)
	{
	  gchar *name = gdk_keyval_name (keysym_tab[vk*4 + state]);
	  if (name == NULL)
	    name = "(none)";
	  g_print ("%s ", name);
	}
      g_print ("\n");
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
set_shift_vks (guchar *key_state,
	       gint    shift)
{
  switch (shift)
    {
    case 0:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case 1:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
      break;
    case 2:
      key_state[VK_SHIFT] = 0;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    case 3:
      key_state[VK_SHIFT] = 0x80;
      key_state[VK_CONTROL] = key_state[VK_MENU] = 0x80;
      break;
    }
}

static void
reset_after_dead (guchar key_state[256])
{
  guchar temp_key_state[256];
  wchar_t wcs[2];

  memmove (temp_key_state, key_state, sizeof (key_state));

  temp_key_state[VK_SHIFT] =
    temp_key_state[VK_CONTROL] =
    temp_key_state[VK_MENU] = 0;

  ToUnicodeEx (VK_SPACE, MapVirtualKey (VK_SPACE, 0),
	       temp_key_state, wcs, G_N_ELEMENTS (wcs),
	       0, _gdk_input_locale);
}

static void
handle_dead (guint  keysym,
	     guint *ksymp)
{
  switch (keysym)
    {
    case '"': /* 0x022 */
      *ksymp = GDK_KEY_dead_diaeresis; break;
    case '\'': /* 0x027 */
      *ksymp = GDK_KEY_dead_acute; break;
    case GDK_KEY_asciicircum: /* 0x05e */
      *ksymp = GDK_KEY_dead_circumflex; break;
    case GDK_KEY_grave:	/* 0x060 */
      *ksymp = GDK_KEY_dead_grave; break;
    case GDK_KEY_asciitilde: /* 0x07e */
      *ksymp = GDK_KEY_dead_tilde; break;
    case GDK_KEY_diaeresis: /* 0x0a8 */
      *ksymp = GDK_KEY_dead_diaeresis; break;
    case GDK_KEY_degree: /* 0x0b0 */
      *ksymp = GDK_KEY_dead_abovering; break;
    case GDK_KEY_acute:	/* 0x0b4 */
      *ksymp = GDK_KEY_dead_acute; break;
    case GDK_KEY_periodcentered: /* 0x0b7 */
      *ksymp = GDK_KEY_dead_abovedot; break;
    case GDK_KEY_cedilla: /* 0x0b8 */
      *ksymp = GDK_KEY_dead_cedilla; break;
    case GDK_KEY_breve:	/* 0x1a2 */
      *ksymp = GDK_KEY_dead_breve; break;
    case GDK_KEY_ogonek: /* 0x1b2 */
      *ksymp = GDK_KEY_dead_ogonek; break;
    case GDK_KEY_caron:	/* 0x1b7 */
      *ksymp = GDK_KEY_dead_caron; break;
    case GDK_KEY_doubleacute: /* 0x1bd */
      *ksymp = GDK_KEY_dead_doubleacute; break;
    case GDK_KEY_abovedot: /* 0x1ff */
      *ksymp = GDK_KEY_dead_abovedot; break;
    case 0x1000384: /* Greek tonos */
      *ksymp = GDK_KEY_dead_acute; break;
    case GDK_KEY_Greek_accentdieresis: /* 0x7ae */
      *ksymp = GDK_KEY_Greek_accentdieresis; break;
    default:
      /* By default use the keysym as such. This takes care of for
       * instance the dead U+09CD (BENGALI VIRAMA) on the ekushey
       * Bengali layout.
       */
      *ksymp = keysym; break;
    }
}

static void
update_keymap (void)
{
  static guint current_serial = 0;
  guchar key_state[256];
  guint scancode;
  guint vk;
  gboolean capslock_tested = FALSE;

  if (keysym_tab != NULL && current_serial == _gdk_keymap_serial)
    return;

  current_serial = _gdk_keymap_serial;

  if (keysym_tab == NULL)
    keysym_tab = g_new (guint, 4*256);

  memset (key_state, 0, sizeof (key_state));

  _gdk_keyboard_has_altgr = FALSE;
  gdk_shift_modifiers = GDK_SHIFT_MASK;

  for (vk = 0; vk < 256; vk++)
    {
      if ((scancode = MapVirtualKey (vk, 0)) == 0 &&
	  vk != VK_DIVIDE)
	keysym_tab[vk*4+0] =
	  keysym_tab[vk*4+1] =
	  keysym_tab[vk*4+2] =
	  keysym_tab[vk*4+3] = GDK_KEY_VoidSymbol;
      else
	{
	  gint shift;

	  if (vk == VK_RSHIFT)
	    _scancode_rshift = scancode;

	  key_state[vk] = 0x80;
	  for (shift = 0; shift < 4; shift++)
	    {
	      guint *ksymp = keysym_tab + vk*4 + shift;
	      
	      set_shift_vks (key_state, shift);

	      *ksymp = 0;

	      /* First, handle those virtual keys that we always want
	       * as special GDK_* keysyms, even if ToAsciiEx might
	       * turn some them into a ASCII character (like TAB and
	       * ESC).
	       */
	      handle_special (vk, ksymp, shift);

	      if (*ksymp == 0)
		{
		  wchar_t wcs[10];
		  gint k;

		  wcs[0] = wcs[1] = 0;
		  k = ToUnicodeEx (vk, scancode, key_state,
				   wcs, G_N_ELEMENTS (wcs),
				   0, _gdk_input_locale);
#if 0
		  g_print ("ToUnicodeEx(%#02x, %d: %d): %d, %04x %04x\n",
			   vk, scancode, shift, k,
			   wcs[0], wcs[1]);
#endif
		  if (k == 1)
		    *ksymp = gdk_unicode_to_keyval (wcs[0]);
		  else if (k == -1)
		    {
		      guint keysym = gdk_unicode_to_keyval (wcs[0]);

		      /* It is a dead key, and it has been stored in
		       * the keyboard layout's state by
		       * ToAsciiEx()/ToUnicodeEx(). Yes, this is an
		       * incredibly silly API! Make the keyboard
		       * layout forget it by calling
		       * ToAsciiEx()/ToUnicodeEx() once more, with the
		       * virtual key code and scancode for the
		       * spacebar, without shift or AltGr. Otherwise
		       * the next call to ToAsciiEx() with a different
		       * key would try to combine with the dead key.
		       */
		      reset_after_dead (key_state);

		      /* Use dead keysyms instead of "undead" ones */
		      handle_dead (keysym, ksymp);
		    }
		  else if (k == 0)
		    {
		      /* Seems to be necessary to "reset" the keyboard layout
		       * in this case, too. Otherwise problems on NT4.
		       */
		      reset_after_dead (key_state);
		    }
		  else
		    {
#if 0
		      GDK_NOTE (EVENTS,
				g_print ("ToUnicodeEx returns %d "
					 "for vk:%02x, sc:%02x%s%s\n",
					 k, vk, scancode,
					 (shift&0x1 ? " shift" : ""),
					 (shift&0x2 ? " altgr" : "")));
#endif
		    }
		}
	      if (*ksymp == 0)
		*ksymp = GDK_KEY_VoidSymbol;
	    }
	  key_state[vk] = 0;

	  /* Check if keyboard has an AltGr key by checking if
	   * the mapping with Control+Alt is different.
	   */
	  if (!_gdk_keyboard_has_altgr)
	    if ((keysym_tab[vk*4 + 2] != GDK_KEY_VoidSymbol &&
		 keysym_tab[vk*4] != keysym_tab[vk*4 + 2]) ||
		(keysym_tab[vk*4 + 3] != GDK_KEY_VoidSymbol &&
		 keysym_tab[vk*4 + 1] != keysym_tab[vk*4 + 3]))
	      _gdk_keyboard_has_altgr = TRUE;
	  
	  if (!capslock_tested)
	    {
	      /* Can we use this virtual key to determine the CapsLock
	       * key behaviour: CapsLock or ShiftLock? If it generates
	       * keysyms for printable characters and has a shifted
	       * keysym that isn't just the upperacase of the
	       * unshifted keysym, check the behaviour of VK_CAPITAL.
	       */
	      if (g_unichar_isgraph (gdk_keyval_to_unicode (keysym_tab[vk*4 + 0])) &&
		  keysym_tab[vk*4 + 1] != keysym_tab[vk*4 + 0] &&
		  g_unichar_isgraph (gdk_keyval_to_unicode (keysym_tab[vk*4 + 1])) &&
		  keysym_tab[vk*4 + 1] != gdk_keyval_to_upper (keysym_tab[vk*4 + 0]))
		{
		  guchar chars[2];
		  
		  capslock_tested = TRUE;
		  
		  key_state[VK_SHIFT] = 0;
		  key_state[VK_CONTROL] = key_state[VK_MENU] = 0;
		  key_state[VK_CAPITAL] = 1;

		  if (ToAsciiEx (vk, scancode, key_state,
				 (LPWORD) chars, 0, _gdk_input_locale) == 1)
		    {
		      if (chars[0] >= GDK_KEY_space &&
			  chars[0] <= GDK_KEY_asciitilde &&
			  chars[0] == keysym_tab[vk*4 + 1])
			{
			  /* CapsLock acts as ShiftLock */
			  gdk_shift_modifiers |= GDK_LOCK_MASK;
			}
		    }
		  key_state[VK_CAPITAL] = 0;
		}    
	    }
	}
    }
  GDK_NOTE (EVENTS, print_keysym_tab ());
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
gdk_win32_keymap_get_direction (GdkKeymap *keymap)
{
  update_keymap ();

  switch (PRIMARYLANGID (LOWORD ((DWORD) (gintptr) _gdk_input_locale)))
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

static gboolean
gdk_win32_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  /* Should we check if the kayboard layouts switchable at the moment
   * cover both directionalities? What does the doc comment in
   * ../x11/gdkkeys-x11.c exactly mean?
   */
  return FALSE;
}

static gboolean
gdk_win32_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return ((GetKeyState (VK_CAPITAL) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return ((GetKeyState (VK_NUMLOCK) & 1) != 0);
}

static gboolean
gdk_win32_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                   guint          keyval,
                                   GdkKeymapKey **keys,
                                   gint          *n_keys)
{
  GArray *retval;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (keys != NULL, FALSE);
  g_return_val_if_fail (n_keys != NULL, FALSE);
  g_return_val_if_fail (keyval != 0, FALSE);
  
  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  /* Accept only the default keymap */
  if (keymap == NULL || keymap == gdk_keymap_get_default ())
    {
      gint vk;
      
      update_keymap ();

      for (vk = 0; vk < 256; vk++)
	{
	  gint i;

	  for (i = 0; i < 4; i++)
	    {
	      if (keysym_tab[vk*4+i] == keyval)
		{
		  GdkKeymapKey key;
		  
		  key.keycode = vk;
		  
		  /* 2 levels (normal, shift), two groups (normal, AltGr) */
		  key.group = i / 2;
		  key.level = i % 2;
		  
		  g_array_append_val (retval, key);
		}
	    }
	}
    }

#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_EVENTS)
    {
      gint i;

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
gdk_win32_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  GArray *key_array;
  GArray *keyval_array;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  if (hardware_keycode <= 0 ||
      hardware_keycode >= 256)
    {
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
  
  /* Accept only the default keymap */
  if (keymap == NULL || keymap == gdk_keymap_get_default ())
    {
      gint i;

      update_keymap ();

      for (i = 0; i < 4; i++)
	{
	  if (key_array)
            {
              GdkKeymapKey key;
	      
              key.keycode = hardware_keycode;
              
              key.group = i / 2;
              key.level = i % 2;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, keysym_tab[hardware_keycode*4+i]);
	}
    }

  if ((key_array && key_array->len > 0) ||
      (keyval_array && keyval_array->len > 0))
    {
      if (keys)
        *keys = (GdkKeymapKey*) key_array->data;

      if (keyvals)
        *keyvals = (guint*) keyval_array->data;

      if (key_array)
        *n_entries = key_array->len;
      else
        *n_entries = keyval_array->len;
    }
  else
    {
      if (keys)
        *keys = NULL;

      if (keyvals)
        *keyvals = NULL;
      
      *n_entries = 0;
    }

  if (key_array)
    g_array_free (key_array, key_array->len > 0 ? FALSE : TRUE);
  if (keyval_array)
    g_array_free (keyval_array, keyval_array->len > 0 ? FALSE : TRUE);

  return *n_entries > 0;
}

static guint
gdk_win32_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  guint sym;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (key->group < 4, 0);
  
  /* Accept only the default keymap */
  if (keymap != NULL && keymap != gdk_keymap_get_default ())
    return 0;

  update_keymap ();
  
  if (key->keycode >= 256 ||
      key->group < 0 || key->group >= 2 ||
      key->level < 0 || key->level >= 2)
    return 0;
  
  sym = keysym_tab[key->keycode*4 + key->group*2 + key->level];
  
  if (sym == GDK_KEY_VoidSymbol)
    return 0;
  else
    return sym;
}

static gboolean
gdk_win32_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  guint tmp_keyval;
  guint *keyvals;
  gint shift_level;
  gboolean ignore_shift = FALSE;
  gboolean ignore_group = FALSE;
      
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);
  
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
  if (keymap != NULL && keymap != gdk_keymap_get_default ())
    return FALSE;

  if (hardware_keycode >= 256)
    return FALSE;

  if (group < 0 || group >= 2)
    return FALSE;

  update_keymap ();

  keyvals = keysym_tab + hardware_keycode*4;

  if ((state & GDK_LOCK_MASK) &&
      (state & GDK_SHIFT_MASK) &&
      ((gdk_shift_modifiers & GDK_LOCK_MASK) ||
       (keyvals[group*2 + 1] == gdk_keyval_to_upper (keyvals[group*2 + 0]))))
    /* Shift always disables ShiftLock. Shift disables CapsLock for
     * keys with lowercase/uppercase letter pairs.
     */
    shift_level = 0;
  else if (state & gdk_shift_modifiers)
    shift_level = 1;
  else
    shift_level = 0;

  /* Drop group and shift if there are no keysymbols on
   * the key for those.
   */
  if (shift_level == 1 &&
      keyvals[group*2 + 1] == GDK_KEY_VoidSymbol &&
      keyvals[group*2] != GDK_KEY_VoidSymbol)
    {
      shift_level = 0;
      ignore_shift = TRUE;
    }

  if (group == 1 &&
      keyvals[2 + shift_level] == GDK_KEY_VoidSymbol &&
      keyvals[0 + shift_level] != GDK_KEY_VoidSymbol)
    {
      group = 0;
      ignore_group = TRUE;
    }

  if (keyvals[group *2 + shift_level] == GDK_KEY_VoidSymbol &&
      keyvals[0 + 0] != GDK_KEY_VoidSymbol)
    {
      shift_level = 0;
      group = 0;
      ignore_group = TRUE;
      ignore_shift = TRUE;
    }

  /* See whether the group and shift level actually mattered
   * to know what to put in consumed_modifiers
   */
  if (keyvals[group*2 + 1] == GDK_KEY_VoidSymbol ||
      keyvals[group*2 + 0] == keyvals[group*2 + 1])
    ignore_shift = TRUE;

  if (keyvals[2 + shift_level] == GDK_KEY_VoidSymbol ||
      keyvals[0 + shift_level] == keyvals[2 + shift_level])
    ignore_group = TRUE;

  tmp_keyval = keyvals[group*2 + shift_level];

  /* If a true CapsLock is toggled, and Shift is not down,
   * and the shifted keysym is the uppercase of the unshifted,
   * use it.
   */
  if (!(gdk_shift_modifiers & GDK_LOCK_MASK) &&
      !(state & GDK_SHIFT_MASK) &&
      (state & GDK_LOCK_MASK))
    {
      guint upper = gdk_keyval_to_upper (tmp_keyval);
      if (upper == keyvals[group*2 + 1])
	tmp_keyval = upper;
    }

  if (keyval)
    *keyval = tmp_keyval;

  if (effective_group)
    *effective_group = group;

  if (level)
    *level = shift_level;

  if (consumed_modifiers)
    {
      *consumed_modifiers =
	(ignore_group ? 0 : GDK_MOD2_MASK) |
	(ignore_shift ? 0 : (GDK_SHIFT_MASK|GDK_LOCK_MASK));
    }
				
#if 0
  GDK_NOTE (EVENTS, g_print ("... group=%d level=%d cmods=%#x keyval=%s\n",
			     group, shift_level, tmp_modifiers, gdk_keyval_name (tmp_keyval)));
#endif

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
gdk_win32_keymap_finalize (GObject *object)
{
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
  keymap_class->get_entries_for_keyval = gdk_win32_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_win32_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_win32_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_win32_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_win32_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_win32_keymap_map_virtual_modifiers;
}
