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

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "gdk.h"

#include "gdkprivate-x11.h"
#include "gdkinternals.h"
#include "gdkkeysyms.h"

#include "config.h"

guint _gdk_keymap_serial = 0;

static gint min_keycode = 0;
static gint max_keycode = 0;

static inline void
update_keyrange (void)
{
  if (max_keycode == 0)
    XDisplayKeycodes (gdk_display, &min_keycode, &max_keycode);
}

#ifdef HAVE_XKB
#include <X11/XKBlib.h>

gboolean _gdk_use_xkb = FALSE;
gint _gdk_xkb_event_type;
static XkbDescPtr xkb_desc = NULL;

static XkbDescPtr
get_xkb (void)
{
  static guint current_serial = 0;

  update_keyrange ();
  
  if (xkb_desc == NULL)
    {
      xkb_desc = XkbGetMap (gdk_display, XkbKeySymsMask, XkbUseCoreKbd);
      if (xkb_desc == NULL)
        g_error ("Failed to get keymap");

      XkbGetNames (gdk_display, XkbGroupNamesMask, xkb_desc);
    }
  else if (current_serial != _gdk_keymap_serial)
    {
      XkbGetUpdatedMap (gdk_display, XkbKeySymsMask, xkb_desc);
      XkbGetNames (gdk_display, XkbGroupNamesMask, xkb_desc);
    }

  current_serial = _gdk_keymap_serial;

  return xkb_desc;
}
#endif /* HAVE_XKB */

/* Whether we were able to turn on detectable-autorepeat using
 * XkbSetDetectableAutorepeat. If FALSE, we'll fall back
 * to checking the next event with XPending().
 */
gboolean _gdk_have_xkb_autorepeat = FALSE;

static KeySym* keymap = NULL;
static gint keysyms_per_keycode = 0;
static XModifierKeymap* mod_keymap = NULL;
static GdkModifierType group_switch_mask = 0;
static PangoDirection current_direction;
static gboolean       have_direction = FALSE;
static GdkKeymap *default_keymap = NULL;

GdkKeymap*
gdk_keymap_get_default (void)
{
  if (default_keymap == NULL)
    default_keymap = g_object_new (gdk_keymap_get_type (), NULL);

  return default_keymap;
}

static void
update_keymaps (void)
{
  static guint current_serial = 0;
  
#ifdef HAVE_XKB
  g_assert (!_gdk_use_xkb);
#endif
  
  if (keymap == NULL ||
      current_serial != _gdk_keymap_serial)
    {
      gint i;
      gint map_size;

      current_serial = _gdk_keymap_serial;

      update_keyrange ();
      
      if (keymap)
        XFree (keymap);

      if (mod_keymap)
        XFreeModifiermap (mod_keymap);
      
      keymap = XGetKeyboardMapping (gdk_display, min_keycode,
                                    max_keycode - min_keycode,
                                    &keysyms_per_keycode);

      mod_keymap = XGetModifierMapping (gdk_display);


      group_switch_mask = 0;

      /* there are 8 modifiers, and the first 3 are shift, shift lock,
       * and control
       */
      map_size = 8 * mod_keymap->max_keypermod;
      i = 3 * mod_keymap->max_keypermod;
      while (i < map_size)
        {
          /* get the key code at this point in the map,
           * see if its keysym is GDK_Mode_switch, if so
           * we have the mode key
           */
          gint keycode = mod_keymap->modifiermap[i];
      
          if (keycode >= min_keycode &&
              keycode <= max_keycode)
            {
              gint j = 0;
              KeySym *syms = keymap + (keycode - min_keycode) * keysyms_per_keycode;
              while (j < keysyms_per_keycode)
                {
                  if (syms[j] == GDK_Mode_switch)
                    {
                      /* This modifier swaps groups */

                      /* GDK_MOD1_MASK is 1 << 3 for example, i.e. the
                       * fourth modifier, i / keyspermod is the modifier
                       * index
                       */
                  
                      group_switch_mask |= (1 << ( i / mod_keymap->max_keypermod));
                      break;
                    }
              
                  ++j;
                }
            }
      
          ++i;
        }
    }
}

static const KeySym*
get_keymap (void)
{
  update_keymaps ();  
  
  return keymap;
}

#if HAVE_XKB
PangoDirection
get_direction (void)
{
  XkbDescRec *xkb = get_xkb ();
  char *name;
  XkbStateRec state_rec;
  PangoDirection result;

  XkbGetState (gdk_display, XkbUseCoreKbd, &state_rec);

  name = gdk_atom_name (xkb->names->groups[state_rec.locked_group]);
  if (g_strcasecmp (name, "arabic") == 0 ||
      g_strcasecmp (name, "hebrew") == 0 ||
      g_strcasecmp (name, "israelian") == 0)
    result = PANGO_DIRECTION_RTL;
  else
    result = PANGO_DIRECTION_LTR;
    
  g_free (name);

  return result;
}

void
_gdk_keymap_state_changed (void)
{
  if (default_keymap)
    {
      PangoDirection new_direction = get_direction ();
      
      if (!have_direction || new_direction != current_direction)
	{
	  have_direction = TRUE;
	  current_direction = new_direction;
	  g_signal_emit_by_name (G_OBJECT (default_keymap), "direction_changed");
	}
    }
}
#endif /* HAVE_XKB */
  
PangoDirection
gdk_keymap_get_direction (GdkKeymap *keymap)
{
#if HAVE_XKB
  if (_gdk_use_xkb)
    {
      if (!have_direction)
	{
	  current_direction = get_direction ();
	  have_direction = TRUE;
	}
  
      return current_direction;
    }
  else
#endif /* HAVE_XKB */
    return PANGO_DIRECTION_LTR;
}

/**
 * gdk_keymap_get_entries_for_keyval:
 * @keymap: a #GdkKeymap, or %NULL to use the default keymap
 * @keyval: a keyval, such as %GDK_a, %GDK_Up, %GDK_Return, etc.
 * @keys: return location for an array of #GdkKeymapKey
 * @n_keys: return location for number of elements in returned array
 * 
 * Obtains a list of keycode/group/level combinations that will
 * generate @keyval. Groups and levels are two kinds of keyboard mode;
 * in general, the level determines whether the top or bottom symbol
 * on a key is used, and the group determines whether the left or
 * right symbol is used. On US keyboards, the shift key changes the
 * keyboard level, and there are no groups. A group switch key might
 * convert a keyboard between Hebrew to English modes, for example.
 * #GdkEventKey contains a %group field that indicates the active
 * keyboard group. The level is computed from the modifier mask.
 * The returned array should be freed
 * with g_free().
 *
 * Return value: %TRUE if keys were found and returned
 **/
gboolean
gdk_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
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

#ifdef HAVE_XKB
  if (_gdk_use_xkb)
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb ();
      gint keycode;
      
      keycode = min_keycode;

      while (keycode <= max_keycode)
        {
          gint max_shift_levels = XkbKeyGroupsWidth (xkb, keycode); /* "key width" */
          gint group = 0;
          gint level = 0;
          gint total_syms = XkbKeyNumSyms (xkb, keycode);
          gint i = 0;
          KeySym *entry;

          /* entry is an array with all syms for group 0, all
           * syms for group 1, etc. and for each group the
           * shift level syms are in order
           */
          entry = XkbKeySymsPtr (xkb, keycode);

          while (i < total_syms)
            {
              /* check out our cool loop invariant */
              g_assert (i == (group * max_shift_levels + level));

              if (entry[i] == keyval)
                {
                  /* Found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;
                  key.group = group;
                  key.level = level;

                  g_array_append_val (retval, key);

                  g_assert (XkbKeySymEntry (xkb, keycode, level, group) == keyval);
                }

              ++level;

              if (level == max_shift_levels)
                {
                  level = 0;
                  ++group;
                }

              ++i;
            }

          ++keycode;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap ();
      gint keycode;
      
      keycode = min_keycode;
      while (keycode < max_keycode)
        {
          const KeySym *syms = map + (keycode - min_keycode) * keysyms_per_keycode;
          gint i = 0;

          while (i < keysyms_per_keycode)
            {
              if (syms[i] == keyval)
                {
                  /* found a match */
                  GdkKeymapKey key;

                  key.keycode = keycode;

                  /* The "classic" non-XKB keymap has 2 levels per group */
                  key.group = i / 2;
                  key.level = i % 2;

                  g_array_append_val (retval, key);
                }
              
              ++i;
            }
          
          ++keycode;
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

/**
 * gdk_keymap_get_entries_for_keycode:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @hardware_keycode: a keycode
 * @keys: return location for array of #GdkKeymapKey, or NULL
 * @keyvals: return location for array of keyvals, or NULL
 * @n_entries: length of @keys and @keyvals
 *
 * Returns the keyvals bound to @hardware_keycode.
 * The Nth #GdkKeymapKey in @keys is bound to the Nth
 * keyval in @keyvals. Free the returned arrays with g_free().
 * When a keycode is pressed by the user, the keyval from
 * this list of entries is selected by considering the effective
 * keyboard group and level. See gdk_keymap_translate_keyboard_state().
 *
 * Returns: %TRUE if there were any entries
 **/
gboolean
gdk_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                    guint          hardware_keycode,
                                    GdkKeymapKey **keys,
                                    guint        **keyvals,
                                    gint          *n_entries)
{
  GArray *key_array;
  GArray *keyval_array;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  update_keyrange ();

  if (hardware_keycode < min_keycode ||
      hardware_keycode > max_keycode)
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
  
#ifdef HAVE_XKB
  if (_gdk_use_xkb)
    {
      /* See sec 15.3.4 in XKB docs */

      XkbDescRec *xkb = get_xkb ();
      gint max_shift_levels;
      gint group = 0;
      gint level = 0;
      gint total_syms;
      gint i = 0;
      KeySym *entry;
      
      max_shift_levels = XkbKeyGroupsWidth (xkb, hardware_keycode); /* "key width" */
      total_syms = XkbKeyNumSyms (xkb, hardware_keycode);

      /* entry is an array with all syms for group 0, all
       * syms for group 1, etc. and for each group the
       * shift level syms are in order
       */
      entry = XkbKeySymsPtr (xkb, hardware_keycode);

      while (i < total_syms)
        {          
          /* check out our cool loop invariant */          
          g_assert (i == (group * max_shift_levels + level));

          if (key_array)
            {
              GdkKeymapKey key;
              
              key.keycode = hardware_keycode;
              key.group = group;
              key.level = level;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, entry[i]);
          
          ++level;
          
          if (level == max_shift_levels)
            {
              level = 0;
              ++group;
            }
          
          ++i;
        }
    }
  else
#endif
    {
      const KeySym *map = get_keymap ();
      const KeySym *syms;
      gint i = 0;

      syms = map + (hardware_keycode - min_keycode) * keysyms_per_keycode;

      while (i < keysyms_per_keycode)
        {
          if (key_array)
            {
              GdkKeymapKey key;
          
              key.keycode = hardware_keycode;
              
              /* The "classic" non-XKB keymap has 2 levels per group */
              key.group = i / 2;
              key.level = i % 2;
              
              g_array_append_val (key_array, key);
            }

          if (keyval_array)
            g_array_append_val (keyval_array, syms[i]);
          
          ++i;
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


/**
 * gdk_keymap_lookup_key:
 * @keymap: a #GdkKeymap or %NULL to use the default keymap
 * @key: a #GdkKeymapKey with keycode, group, and level initialized
 * 
 * Looks up the keyval mapped to a keycode/group/level triplet.
 * If no keyval is bound to @key, returns 0. For normal user input,
 * you want to use gdk_keymap_translate_keyboard_state() instead of
 * this function, since the effective group/level may not be
 * the same as the current keyboard state.
 * 
 * Return value: a keyval, or 0 if none was mapped to the given @key
 **/
guint
gdk_keymap_lookup_key (GdkKeymap          *keymap,
                       const GdkKeymapKey *key)
{
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), 0);
  g_return_val_if_fail (key != NULL, 0);
  g_return_val_if_fail (key->group < 4, 0);
  
#ifdef HAVE_XKB
  if (_gdk_use_xkb)
    {
      XkbDescRec *xkb = get_xkb ();
      
      return XkbKeySymEntry (xkb, key->keycode, key->level, key->group);
    }
  else
#endif
    {
      update_keymaps ();
      
      return XKeycodeToKeysym (gdk_display, key->keycode,
                               key->group * keysyms_per_keycode + key->level);
    }
}

#ifdef HAVE_XKB
/* This is copied straight from XFree86 Xlib, because I needed to
 * add the group and level return. It's unchanged for ease of
 * diff against the Xlib sources; don't reformat it.
 */
static Bool
MyEnhancedXkbTranslateKeyCode(register XkbDescPtr     xkb,
                              KeyCode                 key,
                              register unsigned int   mods,
                              unsigned int *          mods_rtrn,
                              KeySym *                keysym_rtrn,
                              unsigned int *          group_rtrn,
                              unsigned int *          level_rtrn)
{
    XkbKeyTypeRec *type;
    int col,nKeyGroups;
    unsigned preserve,effectiveGroup;
    KeySym *syms;

    if (mods_rtrn!=NULL)
        *mods_rtrn = 0;

    nKeyGroups= XkbKeyNumGroups(xkb,key);
    if ((!XkbKeycodeInRange(xkb,key))||(nKeyGroups==0)) {
        if (keysym_rtrn!=NULL)
            *keysym_rtrn = NoSymbol;
        return False;
    }

    syms = XkbKeySymsPtr(xkb,key);

    /* find the offset of the effective group */
    col = 0;
    effectiveGroup= XkbGroupForCoreState(mods);
    if ( effectiveGroup>=nKeyGroups ) {
        unsigned groupInfo= XkbKeyGroupInfo(xkb,key);
        switch (XkbOutOfRangeGroupAction(groupInfo)) {
            default:
                effectiveGroup %= nKeyGroups;
                break;
            case XkbClampIntoRange:
                effectiveGroup = nKeyGroups-1;
                break;
            case XkbRedirectIntoRange:
                effectiveGroup = XkbOutOfRangeGroupNumber(groupInfo);
                if (effectiveGroup>=nKeyGroups)
                    effectiveGroup= 0;
                break;
        }
    }
    col= effectiveGroup*XkbKeyGroupsWidth(xkb,key);
    type = XkbKeyKeyType(xkb,key,effectiveGroup);

    preserve= 0;
    if (type->map) { /* find the column (shift level) within the group */
        register int i;
        register XkbKTMapEntryPtr entry;
        for (i=0,entry=type->map;i<type->map_count;i++,entry++) {
            if ((entry->active)&&((mods&type->mods.mask)==entry->mods.mask)) {
                col+= entry->level;
                if (type->preserve)
                    preserve= type->preserve[i].mask;

                /* ---- Begin stuff GDK adds to the original Xlib version ---- */
                
                if (level_rtrn)
                  *level_rtrn = entry->level;
                
                /* ---- End stuff GDK adds to the original Xlib version ---- */
                
                break;
            }
        }
    }

    if (keysym_rtrn!=NULL)
        *keysym_rtrn= syms[col];
    if (mods_rtrn) {
        *mods_rtrn= type->mods.mask&(~preserve);

        /* ---- Begin stuff GDK comments out of the original Xlib version ---- */
        /* This is commented out because xkb_info is a private struct */

#if 0
        /* The Motif VTS doesn't get the help callback called if help
         * is bound to Shift+<whatever>, and it appears as though it 
         * is XkbTranslateKeyCode that is causing the problem.  The 
         * core X version of XTranslateKey always OR's in ShiftMask 
         * and LockMask for mods_rtrn, so this "fix" keeps this behavior 
         * and solves the VTS problem.
         */
        if ((xkb->dpy)&&(xkb->dpy->xkb_info)&&
            (xkb->dpy->xkb_info->xlib_ctrls&XkbLC_AlwaysConsumeShiftAndLock)) {            *mods_rtrn|= (ShiftMask|LockMask);
        }
#endif
        
        /* ---- End stuff GDK comments out of the original Xlib version ---- */
    }

    /* ---- Begin stuff GDK adds to the original Xlib version ---- */

    if (group_rtrn)
      *group_rtrn = effectiveGroup;
    
    /* ---- End stuff GDK adds to the original Xlib version ---- */
    
    return (syms[col]!=NoSymbol);
}
#endif /* HAVE_XKB */

/**
 * gdk_keymap_translate_keyboard_state:
 * @keymap: a #GdkKeymap, or %NULL to use the default
 * @hardware_keycode: a keycode
 * @state: a modifier state 
 * @group: active keyboard group
 * @keyval: return location for keyval
 * @effective_group: return location for effective group
 * @level: return location for level
 * @unused_modifiers: return location for modifiers that didn't affect the group or level
 * 
 *
 * Translates the contents of a #GdkEventKey into a keyval, effective
 * group, and level. Modifiers that didn't affect the translation and
 * are thus available for application use are returned in
 * @unused_modifiers.  See gdk_keyval_get_keys() for an explanation of
 * groups and levels.  The @effective_group is the group that was
 * actually used for the translation; some keys such as Enter are not
 * affected by the active keyboard group. The @level is derived from
 * @state. For convenience, #GdkEventKey already contains the translated
 * keyval, so this function isn't as useful as you might think.
 * 
 * Return value: %TRUE if there was a keyval bound to the keycode/state/group
 **/
gboolean
gdk_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                     guint            hardware_keycode,
                                     GdkModifierType  state,
                                     gint             group,
                                     guint           *keyval,
                                     gint            *effective_group,
                                     gint            *level,
                                     GdkModifierType *unused_modifiers)
{
  KeySym tmp_keyval = NoSymbol;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);
  
  if (keyval)
    *keyval = NoSymbol;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (unused_modifiers)
    *unused_modifiers = state;

  update_keyrange ();
  
  if (hardware_keycode < min_keycode ||
      hardware_keycode > max_keycode)
    return FALSE;
  
#ifdef HAVE_XKB
  if (_gdk_use_xkb)
    {
      XkbDescRec *xkb = get_xkb ();

      /* replace bits 13 and 14 with the provided group */
      state &= ~(1 << 13 | 1 << 14);
      state |= group << 13;
      
      MyEnhancedXkbTranslateKeyCode (xkb,
                                     hardware_keycode,
                                     state,
                                     unused_modifiers,
                                     &tmp_keyval,
                                     effective_group,
                                     level);

      if (keyval)
        *keyval = tmp_keyval;
    }
  else
#endif
    {
      gint shift_level;
      
      update_keymaps ();

      if ((state & GDK_SHIFT_MASK) &&
          (state & GDK_LOCK_MASK))
        shift_level = 0; /* shift disables shift lock */
      else if ((state & GDK_SHIFT_MASK) ||
               (state & GDK_LOCK_MASK))
        shift_level = 1;
      else
        shift_level = 0;

      tmp_keyval = XKeycodeToKeysym (gdk_display, hardware_keycode,
                                     group * keysyms_per_keycode + shift_level);
      
      if (keyval)
        *keyval = tmp_keyval;

      if (unused_modifiers)
        {
          *unused_modifiers = state;
          *unused_modifiers &= ~(GDK_SHIFT_MASK | GDK_LOCK_MASK | group_switch_mask);
        }
      
      if (effective_group)
        *effective_group = (state & group_switch_mask) ? 1 : 0;

      if (level)
        *level = shift_level;
    }

  return tmp_keyval != NoSymbol;
}


/* Key handling not part of the keymap */

gchar*
gdk_keyval_name (guint	      keyval)
{
  return XKeysymToString (keyval);
}

guint
gdk_keyval_from_name (const gchar *keyval_name)
{
  g_return_val_if_fail (keyval_name != NULL, 0);
  
  return XStringToKeysym (keyval_name);
}

#ifdef HAVE_XCONVERTCASE
void
gdk_keyval_convert_case (guint symbol,
			 guint *lower,
			 guint *upper)
{
  KeySym xlower = 0;
  KeySym xupper = 0;

  if (symbol)
    XConvertCase (symbol, &xlower, &xupper);

  if (lower)
    *lower = xlower;
  if (upper)
    *upper = xupper;
}  
#endif /* HAVE_XCONVERTCASE */
