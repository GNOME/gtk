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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkkeys.h"

#include "gdkdisplay.h"


/**
 * SECTION:keys
 * @Short_description: Functions for manipulating keyboard codes
 * @Title: Key Values
 *
 * Key values are the codes which are sent whenever a key is pressed or released.
 * They appear in the #GdkEventKey.keyval field of the
 * #GdkEventKey structure, which is passed to signal handlers for the
 * #GtkWidget::key-press-event and #GtkWidget::key-release-event signals.
 * The complete list of key values can be found in the
 * <filename>&lt;gdk/gdkkeysyms.h&gt;</filename> header file.
 *
 * Key values are regularly updated from the upstream X.org X11 implementation,
 * so new values are added regularly. They will be prefixed with GDK_KEY_ rather
 * than XF86XK_ or XK_ (for older symbols).
 *
 * Key values can be converted into a string representation using
 * gdk_keyval_name(). The reverse function, converting a string to a key value,
 * is provided by gdk_keyval_from_name().
 *
 * The case of key values can be determined using gdk_keyval_is_upper() and
 * gdk_keyval_is_lower(). Key values can be converted to upper or lower case
 * using gdk_keyval_to_upper() and gdk_keyval_to_lower().
 *
 * When it makes sense, key values can be converted to and from
 * Unicode characters with gdk_keyval_to_unicode() and gdk_unicode_to_keyval().
 *
 * <para id="key-group-explanation">
 * One #GdkKeymap object exists for each user display. gdk_keymap_get_default()
 * returns the #GdkKeymap for the default display; to obtain keymaps for other
 * displays, use gdk_keymap_get_for_display(). A keymap
 * is a mapping from #GdkKeymapKey to key values. You can think of a #GdkKeymapKey
 * as a representation of a symbol printed on a physical keyboard key. That is, it
 * contains three pieces of information. First, it contains the hardware keycode;
 * this is an identifying number for a physical key. Second, it contains the
 * <firstterm>level</firstterm> of the key. The level indicates which symbol on the
 * key will be used, in a vertical direction. So on a standard US keyboard, the key
 * with the number "1" on it also has the exclamation point ("!") character on
 * it. The level indicates whether to use the "1" or the "!" symbol.  The letter
 * keys are considered to have a lowercase letter at level 0, and an uppercase
 * letter at level 1, though only the uppercase letter is printed.  Third, the
 * #GdkKeymapKey contains a group; groups are not used on standard US keyboards,
 * but are used in many other countries. On a keyboard with groups, there can be 3
 * or 4 symbols printed on a single key. The group indicates movement in a
 * horizontal direction. Usually groups are used for two different languages.  In
 * group 0, a key might have two English characters, and in group 1 it might have
 * two Hebrew characters. The Hebrew characters will be printed on the key next to
 * the English characters.
 * </para>
 *
 * In order to use a keymap to interpret a key event, it's necessary to first
 * convert the keyboard state into an effective group and level. This is done via a
 * set of rules that varies widely according to type of keyboard and user
 * configuration. The function gdk_keymap_translate_keyboard_state() accepts a
 * keyboard state -- consisting of hardware keycode pressed, active modifiers, and
 * active group -- applies the appropriate rules, and returns the group/level to be
 * used to index the keymap, along with the modifiers which did not affect the
 * group and level. i.e. it returns "unconsumed modifiers." The keyboard group may
 * differ from the effective group used for keymap lookups because some keys don't
 * have multiple groups - e.g. the Enter key is always in group 0 regardless of
 * keyboard state.
 *
 * Note that gdk_keymap_translate_keyboard_state() also returns the keyval, i.e. it
 * goes ahead and performs the keymap lookup in addition to telling you which
 * effective group/level values were used for the lookup. #GdkEventKey already
 * contains this keyval, however, so you don't normally need to call
 * gdk_keymap_translate_keyboard_state() just to get the keyval.
 */


enum {
  DIRECTION_CHANGED,
  KEYS_CHANGED,
  STATE_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GdkKeymap, gdk_keymap, G_TYPE_OBJECT)

static void
gdk_keymap_class_init (GdkKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  /**
   * GdkKeymap::direction-changed:
   * @keymap: the object on which the signal is emitted
   * 
   * The ::direction-changed signal gets emitted when the direction of
   * the keymap changes. 
   *
   * Since: 2.0
   */
  signals[DIRECTION_CHANGED] =
    g_signal_new ("direction-changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkKeymapClass, direction_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);
  /**
   * GdkKeymap::keys-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::keys-changed signal is emitted when the mapping represented by
   * @keymap changes.
   *
   * Since: 2.2
   */
  signals[KEYS_CHANGED] =
    g_signal_new ("keys-changed",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GdkKeymapClass, keys_changed),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE,
		  0);

  /**
   * GdkKeymap::state-changed:
   * @keymap: the object on which the signal is emitted
   *
   * The ::state-changed signal is emitted when the state of the
   * keyboard changes, e.g when Caps Lock is turned on or off.
   * See gdk_keymap_get_caps_lock_state().
   *
   * Since: 2.16
   */
  signals[STATE_CHANGED] =
    g_signal_new ("state_changed",
                  G_OBJECT_CLASS_TYPE (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkKeymapClass, state_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 
                  0);
}

static void
gdk_keymap_init (GdkKeymap *keymap)
{
}

/* Other key-handling stuff
 */

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"

/* compatibility function from X11R6.3, since XConvertCase is not
 * supplied by X11R5.
 */
/**
 * gdk_keyval_convert_case:
 * @symbol: a keyval
 * @lower: (out): return location for lowercase version of @symbol
 * @upper: (out): return location for uppercase version of @symbol
 *
 * Obtains the upper- and lower-case versions of the keyval @symbol.
 * Examples of keyvals are #GDK_KEY_a, #GDK_KEY_Enter, #GDK_KEY_F1, etc.
 *
 **/
void
gdk_keyval_convert_case (guint symbol,
			 guint *lower,
			 guint *upper)
{
  guint xlower = symbol;
  guint xupper = symbol;

  /* Check for directly encoded 24-bit UCS characters: */
  if ((symbol & 0xff000000) == 0x01000000)
    {
      if (lower)
	*lower = gdk_unicode_to_keyval (g_unichar_tolower (symbol & 0x00ffffff));
      if (upper)
	*upper = gdk_unicode_to_keyval (g_unichar_toupper (symbol & 0x00ffffff));
      return;
    }

  switch (symbol >> 8)
    {
    case 0: /* Latin 1 */
      if ((symbol >= GDK_KEY_A) && (symbol <= GDK_KEY_Z))
	xlower += (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_a) && (symbol <= GDK_KEY_z))
	xupper -= (GDK_KEY_a - GDK_KEY_A);
      else if ((symbol >= GDK_KEY_Agrave) && (symbol <= GDK_KEY_Odiaeresis))
	xlower += (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_agrave) && (symbol <= GDK_KEY_odiaeresis))
	xupper -= (GDK_KEY_agrave - GDK_KEY_Agrave);
      else if ((symbol >= GDK_KEY_Ooblique) && (symbol <= GDK_KEY_Thorn))
	xlower += (GDK_KEY_oslash - GDK_KEY_Ooblique);
      else if ((symbol >= GDK_KEY_oslash) && (symbol <= GDK_KEY_thorn))
	xupper -= (GDK_KEY_oslash - GDK_KEY_Ooblique);
      break;
      
    case 1: /* Latin 2 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol == GDK_KEY_Aogonek)
	xlower = GDK_KEY_aogonek;
      else if (symbol >= GDK_KEY_Lstroke && symbol <= GDK_KEY_Sacute)
	xlower += (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_Scaron && symbol <= GDK_KEY_Zacute)
	xlower += (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_Zcaron && symbol <= GDK_KEY_Zabovedot)
	xlower += (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol == GDK_KEY_aogonek)
	xupper = GDK_KEY_Aogonek;
      else if (symbol >= GDK_KEY_lstroke && symbol <= GDK_KEY_sacute)
	xupper -= (GDK_KEY_lstroke - GDK_KEY_Lstroke);
      else if (symbol >= GDK_KEY_scaron && symbol <= GDK_KEY_zacute)
	xupper -= (GDK_KEY_scaron - GDK_KEY_Scaron);
      else if (symbol >= GDK_KEY_zcaron && symbol <= GDK_KEY_zabovedot)
	xupper -= (GDK_KEY_zcaron - GDK_KEY_Zcaron);
      else if (symbol >= GDK_KEY_Racute && symbol <= GDK_KEY_Tcedilla)
	xlower += (GDK_KEY_racute - GDK_KEY_Racute);
      else if (symbol >= GDK_KEY_racute && symbol <= GDK_KEY_tcedilla)
	xupper -= (GDK_KEY_racute - GDK_KEY_Racute);
      break;
      
    case 2: /* Latin 3 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Hstroke && symbol <= GDK_KEY_Hcircumflex)
	xlower += (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_Gbreve && symbol <= GDK_KEY_Jcircumflex)
	xlower += (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_hstroke && symbol <= GDK_KEY_hcircumflex)
	xupper -= (GDK_KEY_hstroke - GDK_KEY_Hstroke);
      else if (symbol >= GDK_KEY_gbreve && symbol <= GDK_KEY_jcircumflex)
	xupper -= (GDK_KEY_gbreve - GDK_KEY_Gbreve);
      else if (symbol >= GDK_KEY_Cabovedot && symbol <= GDK_KEY_Scircumflex)
	xlower += (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      else if (symbol >= GDK_KEY_cabovedot && symbol <= GDK_KEY_scircumflex)
	xupper -= (GDK_KEY_cabovedot - GDK_KEY_Cabovedot);
      break;
      
    case 3: /* Latin 4 */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Rcedilla && symbol <= GDK_KEY_Tslash)
	xlower += (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol >= GDK_KEY_rcedilla && symbol <= GDK_KEY_tslash)
	xupper -= (GDK_KEY_rcedilla - GDK_KEY_Rcedilla);
      else if (symbol == GDK_KEY_ENG)
	xlower = GDK_KEY_eng;
      else if (symbol == GDK_KEY_eng)
	xupper = GDK_KEY_ENG;
      else if (symbol >= GDK_KEY_Amacron && symbol <= GDK_KEY_Umacron)
	xlower += (GDK_KEY_amacron - GDK_KEY_Amacron);
      else if (symbol >= GDK_KEY_amacron && symbol <= GDK_KEY_umacron)
	xupper -= (GDK_KEY_amacron - GDK_KEY_Amacron);
      break;
      
    case 6: /* Cyrillic */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Serbian_DJE && symbol <= GDK_KEY_Serbian_DZE)
	xlower -= (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Serbian_dje && symbol <= GDK_KEY_Serbian_dze)
	xupper += (GDK_KEY_Serbian_DJE - GDK_KEY_Serbian_dje);
      else if (symbol >= GDK_KEY_Cyrillic_YU && symbol <= GDK_KEY_Cyrillic_HARDSIGN)
	xlower -= (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      else if (symbol >= GDK_KEY_Cyrillic_yu && symbol <= GDK_KEY_Cyrillic_hardsign)
	xupper += (GDK_KEY_Cyrillic_YU - GDK_KEY_Cyrillic_yu);
      break;
      
    case 7: /* Greek */
      /* Assume the KeySym is a legal value (ignore discontinuities) */
      if (symbol >= GDK_KEY_Greek_ALPHAaccent && symbol <= GDK_KEY_Greek_OMEGAaccent)
	xlower += (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_alphaaccent && symbol <= GDK_KEY_Greek_omegaaccent &&
	       symbol != GDK_KEY_Greek_iotaaccentdieresis &&
	       symbol != GDK_KEY_Greek_upsilonaccentdieresis)
	xupper -= (GDK_KEY_Greek_alphaaccent - GDK_KEY_Greek_ALPHAaccent);
      else if (symbol >= GDK_KEY_Greek_ALPHA && symbol <= GDK_KEY_Greek_OMEGA)
	xlower += (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      else if (symbol >= GDK_KEY_Greek_alpha && symbol <= GDK_KEY_Greek_omega &&
	       symbol != GDK_KEY_Greek_finalsmallsigma)
	xupper -= (GDK_KEY_Greek_alpha - GDK_KEY_Greek_ALPHA);
      break;
    }

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
}
#endif

/**
 * gdk_keyval_to_upper:
 * @keyval: a key value.
 *
 * Converts a key value to upper case, if applicable.
 *
 * Returns: the upper case form of @keyval, or @keyval itself if it is already
 *   in upper case or it is not subject to case conversion.
 */
guint
gdk_keyval_to_upper (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, NULL, &result);

  return result;
}

/**
 * gdk_keyval_to_lower:
 * @keyval: a key value.
 *
 * Converts a key value to lower case, if applicable.
 *
 * Returns: the lower case form of @keyval, or @keyval itself if it is already
 *  in lower case or it is not subject to case conversion.
 */
guint
gdk_keyval_to_lower (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, &result, NULL);

  return result;
}

/**
 * gdk_keyval_is_upper:
 * @keyval: a key value.
 *
 * Returns %TRUE if the given key value is in upper case.
 *
 * Returns: %TRUE if @keyval is in upper case, or if @keyval is not subject to
 *  case conversion.
 */
gboolean
gdk_keyval_is_upper (guint keyval)
{
  if (keyval)
    {
      guint upper_val = 0;
      
      gdk_keyval_convert_case (keyval, NULL, &upper_val);
      return upper_val == keyval;
    }
  return FALSE;
}

/**
 * gdk_keyval_is_lower:
 * @keyval: a key value.
 *
 * Returns %TRUE if the given key value is in lower case.
 *
 * Returns: %TRUE if @keyval is in lower case, or if @keyval is not
 *   subject to case conversion.
 */
gboolean
gdk_keyval_is_lower (guint keyval)
{
  if (keyval)
    {
      guint lower_val = 0;
      
      gdk_keyval_convert_case (keyval, &lower_val, NULL);
      return lower_val == keyval;
    }
  return FALSE;
}

/** 
 * gdk_keymap_get_default:
 *
 * Returns the #GdkKeymap attached to the default display.
 *
 * Returns: the #GdkKeymap attached to the default display.
 */
GdkKeymap*
gdk_keymap_get_default (void)
{
  return gdk_keymap_get_for_display (gdk_display_get_default ());
}
