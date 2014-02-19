/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#include <locale.h>
#include <stdlib.h>

#include "gdk/gdk.h"

#include "gtkprivate.h"
#include "gtkresources.h"


#if !defined G_OS_WIN32 && !(defined GDK_WINDOWING_QUARTZ && defined QUARTZ_RELOCATION)

const gchar *
_gtk_get_datadir (void)
{
  return GTK_DATADIR;
}

const gchar *
_gtk_get_libdir (void)
{
  return GTK_LIBDIR;
}

const gchar *
_gtk_get_sysconfdir (void)
{
  return GTK_SYSCONFDIR;
}

const gchar *
_gtk_get_localedir (void)
{
  return GTK_LOCALEDIR;
}

const gchar *
_gtk_get_data_prefix (void)
{
  return GTK_DATA_PREFIX;
}

#endif

/* _gtk_get_lc_ctype:
 *
 * Return the Unix-style locale string for the language currently in
 * effect. On Unix systems, this is the return value from
 * `setlocale(LC_CTYPE, NULL)`, and the user can
 * affect this through the environment variables LC_ALL, LC_CTYPE or
 * LANG (checked in that order). The locale strings typically is in
 * the form lang_COUNTRY, where lang is an ISO-639 language code, and
 * COUNTRY is an ISO-3166 country code. For instance, sv_FI for
 * Swedish as written in Finland or pt_BR for Portuguese as written in
 * Brazil.
 *
 * On Windows, the C library doesn’t use any such environment
 * variables, and setting them won’t affect the behaviour of functions
 * like ctime(). The user sets the locale through the Regional Options
 * in the Control Panel. The C library (in the setlocale() function)
 * does not use country and language codes, but country and language
 * names spelled out in English.
 * However, this function does check the above environment
 * variables, and does return a Unix-style locale string based on
 * either said environment variables or the thread’s current locale.
 *
 * Returns: a dynamically allocated string, free with g_free().
 */

gchar *
_gtk_get_lc_ctype (void)
{
#ifdef G_OS_WIN32
  /* Somebody might try to set the locale for this process using the
   * LANG or LC_ environment variables. The Microsoft C library
   * doesn't know anything about them. You set the locale in the
   * Control Panel. Setting these env vars won't have any affect on
   * locale-dependent C library functions like ctime(). But just for
   * kicks, do obey LC_ALL, LC_CTYPE and LANG in GTK. (This also makes
   * it easier to test GTK and Pango in various default languages, you
   * don't have to clickety-click in the Control Panel, you can simply
   * start the program with LC_ALL=something on the command line.)
   */
  gchar *p;

  p = getenv ("LC_ALL");
  if (p != NULL)
    return g_strdup (p);

  p = getenv ("LC_CTYPE");
  if (p != NULL)
    return g_strdup (p);

  p = getenv ("LANG");
  if (p != NULL)
    return g_strdup (p);

  return g_win32_getlocale ();
#else
  return g_strdup (setlocale (LC_CTYPE, NULL));
#endif
}

gboolean
_gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
                                  GValue                *return_accu,
                                  const GValue          *handler_return,
                                  gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;

  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;

  return continue_emission;
}

gboolean
_gtk_single_string_accumulator (GSignalInvocationHint *ihint,
				GValue                *return_accu,
				const GValue          *handler_return,
				gpointer               dummy)
{
  gboolean continue_emission;
  const gchar *str;

  str = g_value_get_string (handler_return);
  g_value_set_string (return_accu, str);
  continue_emission = str == NULL;

  return continue_emission;
}

GdkModifierType
_gtk_replace_virtual_modifiers (GdkKeymap       *keymap,
                                GdkModifierType  modifiers)
{
  GdkModifierType result = 0;
  gint            i;

  g_return_val_if_fail (GDK_IS_KEYMAP (keymap), 0);

  for (i = 0; i < 8; i++) /* SHIFT...MOD5 */
    {
      GdkModifierType real = 1 << i;

      if (modifiers & real)
        {
          GdkModifierType virtual = real;

          gdk_keymap_add_virtual_modifiers (keymap, &virtual);

          if (virtual == real)
            result |= virtual;
          else
            result |= virtual & ~real;
        }
    }

  return result;
}

GdkModifierType
_gtk_get_primary_accel_mod (void)
{
  static GdkModifierType primary = 0;

  if (! primary)
    {
      GdkDisplay *display = gdk_display_get_default ();

      primary = gdk_keymap_get_modifier_mask (gdk_keymap_get_for_display (display),
                                              GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR);
      primary = _gtk_replace_virtual_modifiers (gdk_keymap_get_for_display (display),
                                                primary);
    }

  return primary;
}

gboolean
_gtk_translate_keyboard_accel_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     GdkModifierType  accel_mask,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *consumed_modifiers)
{
  GdkModifierType shift_group_mask;
  gboolean group_mask_disabled = FALSE;
  gboolean retval;

  /* if the group-toggling modifier is part of the accel mod mask, and
   * it is active, disable it for matching
   */
  shift_group_mask = gdk_keymap_get_modifier_mask (keymap,
                                                   GDK_MODIFIER_INTENT_SHIFT_GROUP);
  if (accel_mask & state & shift_group_mask)
    {
      state &= ~shift_group_mask;
      group = 0;
      group_mask_disabled = TRUE;
    }

  retval = gdk_keymap_translate_keyboard_state (keymap,
                                                hardware_keycode, state, group,
                                                keyval,
                                                effective_group, level,
                                                consumed_modifiers);

  /* add back the group mask, we want to match against the modifier,
   * but not against the keyval from its group
   */
  if (group_mask_disabled)
    {
      if (effective_group)
        *effective_group = 1;

      if (consumed_modifiers)
        *consumed_modifiers &= ~shift_group_mask;
    }

  return retval;
}

static gpointer
register_resources (gpointer data)
{
  _gtk_register_resource ();
  return NULL;
}

void
_gtk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}
