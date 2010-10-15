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

guint
gdk_keyval_to_upper (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, NULL, &result);

  return result;
}

guint
gdk_keyval_to_lower (guint keyval)
{
  guint result;
  
  gdk_keyval_convert_case (keyval, &result, NULL);

  return result;
}

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
 * @returns: the #GdkKeymap attached to the default display.
 *
 * Returns the #GdkKeymap attached to the default display.
 **/
GdkKeymap*
gdk_keymap_get_default (void)
{
  return gdk_keymap_get_for_display (gdk_display_get_default ());
}
