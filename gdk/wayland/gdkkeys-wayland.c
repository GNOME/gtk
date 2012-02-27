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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

#include "gdk.h"
#include "gdkwayland.h"

#include "gdkprivate-wayland.h"
#include "gdkinternals.h"
#include "gdkdisplay-wayland.h"
#include "gdkkeysprivate.h"

#include <X11/extensions/XKBcommon.h>

typedef struct _GdkWaylandKeymap          GdkWaylandKeymap;
typedef struct _GdkWaylandKeymapClass     GdkWaylandKeymapClass;

struct _GdkWaylandKeymap
{
  GdkKeymap parent_instance;
  GdkModifierType modmap[8];
  struct xkb_desc *xkb;
};

struct _GdkWaylandKeymapClass
{
  GdkKeymapClass parent_class;
};

#define GDK_TYPE_WAYLAND_KEYMAP          (_gdk_wayland_keymap_get_type ())
#define GDK_WAYLAND_KEYMAP(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_KEYMAP, GdkWaylandKeymap))
#define GDK_IS_WAYLAND_KEYMAP(object)    (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_KEYMAP))

G_DEFINE_TYPE (GdkWaylandKeymap, _gdk_wayland_keymap, GDK_TYPE_KEYMAP)

static void
gdk_wayland_keymap_finalize (GObject *object)
{
  G_OBJECT_CLASS (_gdk_wayland_keymap_parent_class)->finalize (object);
}

static PangoDirection
gdk_wayland_keymap_get_direction (GdkKeymap *keymap)
{
    return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_wayland_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
    return FALSE;
}

static gboolean
gdk_wayland_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_wayland_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_wayland_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
					   guint          keyval,
					   GdkKeymapKey **keys,
					   gint          *n_keys)
{
  GArray *retval;
  uint32_t keycode;
  struct xkb_desc *xkb;

  xkb = GDK_WAYLAND_KEYMAP (keymap)->xkb;
  keycode = xkb->min_key_code;

  retval = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  for (keycode = xkb->min_key_code; keycode <= xkb->max_key_code; keycode++)
    {
      gint max_shift_levels = XkbKeyGroupsWidth (xkb, keycode);

      gint group = 0;
      gint level = 0;
      gint total_syms = XkbKeyNumSyms (xkb, keycode);
      gint i = 0;
      uint32_t *entry;

      /* entry is an array with all syms for group 0, all
       * syms for group 1, etc. and for each group the
       * shift level syms are in order
       */
      entry = XkbKeySymsPtr (xkb, keycode);

      for (i = 0; i < total_syms; i++)
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

	      g_assert (XkbKeySymEntry (xkb, keycode, level, group) ==
			keyval);
	    }

	  level++;

	  if (level == max_shift_levels)
	    {
	      level = 0;
	      group++;
	    }
	}
    }

  *n_keys = retval->len;
  *keys = (GdkKeymapKey *) g_array_free (retval, FALSE);

  return *n_keys > 0;
}

static gboolean
gdk_wayland_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
					    guint          hardware_keycode,
					    GdkKeymapKey **keys,
					    guint        **keyvals,
					    gint          *n_entries)
{
  GArray *key_array;
  GArray *keyval_array;
  struct xkb_desc *xkb;
  gint max_shift_levels;
  gint group = 0;
  gint level = 0;
  gint total_syms;
  gint i;
  uint32_t *entry;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (n_entries != NULL, FALSE);

  xkb = GDK_WAYLAND_KEYMAP (keymap)->xkb;

  if (hardware_keycode < xkb->min_key_code ||
      hardware_keycode > xkb->max_key_code)
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

  /* See sec 15.3.4 in XKB docs */
  max_shift_levels = XkbKeyGroupsWidth (xkb, hardware_keycode);
  total_syms = XkbKeyNumSyms (xkb, hardware_keycode);

  /* entry is an array with all syms for group 0, all
   * syms for group 1, etc. and for each group the
   * shift level syms are in order
   */
  entry = XkbKeySymsPtr (xkb, hardware_keycode);

  for (i = 0; i < total_syms; i++)
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
    }

  *n_entries = 0;

  if (keys)
    {
      *n_entries = key_array->len;
      *keys = (GdkKeymapKey*) g_array_free (key_array, FALSE);
    }

  if (keyvals)
    {
      *n_entries = keyval_array->len;
      *keyvals = (guint*) g_array_free (keyval_array, FALSE);
    }

  return *n_entries > 0;
}

static guint
gdk_wayland_keymap_lookup_key (GdkKeymap          *keymap,
			       const GdkKeymapKey *key)
{
  struct xkb_desc *xkb;

  xkb = GDK_WAYLAND_KEYMAP (keymap)->xkb;

  return XkbKeySymEntry (xkb, key->keycode, key->level, key->group);
}

/* This is copied straight from XFree86 Xlib, to:
 *  - add the group and level return.
 *  - change the interpretation of mods_rtrn as described
 *    in the docs for gdk_keymap_translate_keyboard_state()
 * It's unchanged for ease of diff against the Xlib sources; don't
 * reformat it.
 */
static int
MyEnhancedXkbTranslateKeyCode(struct xkb_desc *       xkb,
                              KeyCode                 key,
                              unsigned int            mods,
                              unsigned int *          mods_rtrn,
                              uint32_t *              keysym_rtrn,
                              int *                   group_rtrn,
                              int *                   level_rtrn)
{
    struct xkb_key_type *type;
    int col,nKeyGroups;
    unsigned preserve,effectiveGroup;
    uint32_t *syms;

    if (mods_rtrn!=NULL)
        *mods_rtrn = 0;

    nKeyGroups= XkbKeyNumGroups(xkb,key);
    if ((!XkbKeycodeInRange(xkb,key))||(nKeyGroups==0)) {
        if (keysym_rtrn!=NULL)
            *keysym_rtrn = 0;
        return 0;
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
        struct xkb_kt_map_entry *entry;
        /* ---- Begin section modified for GDK  ---- */
        int found = 0;

        for (i=0,entry=type->map;i<type->map_count;i++,entry++) {
            if (mods_rtrn) {
                int bits = 0;
                unsigned long tmp = entry->mods.mask;
                while (tmp) {
                    if ((tmp & 1) == 1)
                        bits++;
                    tmp >>= 1;
                }
                /* We always add one-modifiers levels to mods_rtrn since
                 * they can't wipe out bits in the state unless the
                 * level would be triggered. But return other modifiers
                 *
                 */
                if (bits == 1 || (mods&type->mods.mask)==entry->mods.mask)
                    *mods_rtrn |= entry->mods.mask;
            }

            if (!found&&entry->active&&((mods&type->mods.mask)==entry->mods.mask)) {
                col+= entry->level;
                if (type->preserve)
                    preserve= type->preserve[i].mask;

                if (level_rtrn)
                  *level_rtrn = entry->level;

                found = 1;
            }
        }
        /* ---- End section modified for GDK ---- */
    }

    if (keysym_rtrn!=NULL)
        *keysym_rtrn= syms[col];
    if (mods_rtrn) {
        /* ---- Begin section modified for GDK  ---- */
        *mods_rtrn &= ~preserve;
        /* ---- End section modified for GDK ---- */

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

    return (syms[col] != 0);
}

static gboolean
gdk_wayland_keymap_translate_keyboard_state (GdkKeymap       *keymap,
					     guint            hardware_keycode,
					     GdkModifierType  state,
					     gint             group,
					     guint           *keyval,
					     gint            *effective_group,
					     gint            *level,
					     GdkModifierType *consumed_modifiers)
{
  GdkWaylandKeymap *wayland_keymap;
  uint32_t tmp_keyval = 0;
  guint tmp_modifiers;
  struct xkb_desc *xkb;

  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);

  wayland_keymap = GDK_WAYLAND_KEYMAP (keymap);
  xkb = wayland_keymap->xkb;

  if (keyval)
    *keyval = 0;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  if (hardware_keycode < xkb->min_key_code ||
      hardware_keycode > xkb->max_key_code)
    return FALSE;


  /* replace bits 13 and 14 with the provided group */
  state &= ~(1 << 13 | 1 << 14);
  state |= group << 13;

  MyEnhancedXkbTranslateKeyCode (xkb,
				 hardware_keycode,
				 state,
				 &tmp_modifiers,
				 &tmp_keyval,
				 effective_group,
				 level);

  if (state & ~tmp_modifiers & XKB_COMMON_LOCK_MASK)
    tmp_keyval = gdk_keyval_to_upper (tmp_keyval);

  /* We need to augment the consumed modifiers with LockMask, since
   * we handle that ourselves, and also with the group bits
   */
  tmp_modifiers |= XKB_COMMON_LOCK_MASK | 1 << 13 | 1 << 14;


  if (consumed_modifiers)
    *consumed_modifiers = tmp_modifiers;

  if (keyval)
    *keyval = tmp_keyval;

  return tmp_keyval != 0;
}


static void
update_modmap (GdkWaylandKeymap *wayland_keymap)
{
  static struct {
    const gchar *name;
    uint32_t atom;
    GdkModifierType mask;
  } vmods[] = {
    { "Meta", 0, GDK_META_MASK },
    { "Super", 0, GDK_SUPER_MASK },
    { "Hyper", 0, GDK_HYPER_MASK },
    { NULL, 0, 0 }
  };

  gint i, j, k;

  if (!vmods[0].atom)
    for (i = 0; vmods[i].name; i++)
      vmods[i].atom = xkb_intern_atom(vmods[i].name);

  for (i = 0; i < 8; i++)
    wayland_keymap->modmap[i] = 1 << i;

  for (i = 0; i < XkbNumVirtualMods; i++)
    {
      for (j = 0; vmods[j].atom; j++)
	{
	  if (wayland_keymap->xkb->names->vmods[i] == vmods[j].atom)
	    {
	      for (k = 0; k < 8; k++)
		{
		  if (wayland_keymap->xkb->server->vmods[i] & (1 << k))
		    wayland_keymap->modmap[k] |= vmods[j].mask;
		}
	    }
	}
    }
}

static void
gdk_wayland_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
					  GdkModifierType *state)
{
  GdkWaylandKeymap *wayland_keymap;
  int i;

  wayland_keymap = GDK_WAYLAND_KEYMAP (keymap);

  for (i = 4; i < 8; i++)
    {
      if ((1 << i) & *state)
	{
	  if (wayland_keymap->modmap[i] & GDK_SUPER_MASK)
	    *state |= GDK_SUPER_MASK;
	  if (wayland_keymap->modmap[i] & GDK_HYPER_MASK)
	    *state |= GDK_HYPER_MASK;
	  if (wayland_keymap->modmap[i] & GDK_META_MASK)
	    *state |= GDK_META_MASK;
	}
    }
}

static gboolean
gdk_wayland_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
					  GdkModifierType *state)
{
  const guint vmods[] = {
    GDK_SUPER_MASK, GDK_HYPER_MASK, GDK_META_MASK
  };
  int i, j;
  GdkWaylandKeymap *wayland_keymap;
  gboolean retval = TRUE;

  wayland_keymap = GDK_WAYLAND_KEYMAP (keymap);

  for (j = 0; j < 3; j++)
    {
      if (*state & vmods[j])
	{
	  for (i = 4; i < 8; i++)
	    {
	      if (wayland_keymap->modmap[i] & vmods[j])
		{
		  if (*state & (1 << i))
		    retval = FALSE;
		  else
		    *state |= 1 << i;
		}
	    }
	}
    }

  return retval;
}

static void
_gdk_wayland_keymap_class_init (GdkWaylandKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_wayland_keymap_finalize;

  keymap_class->get_direction = gdk_wayland_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_wayland_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_wayland_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_wayland_keymap_get_num_lock_state;
  keymap_class->get_entries_for_keyval = gdk_wayland_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_wayland_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_wayland_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_wayland_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_wayland_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_wayland_keymap_map_virtual_modifiers;
}

static void
_gdk_wayland_keymap_init (GdkWaylandKeymap *keymap)
{
}

static void
update_keymaps (GdkWaylandKeymap *keymap)
{
  struct xkb_desc *xkb = keymap->xkb;
  gint keycode, total_syms, i, modifier;
  uint32_t *entry;
  guint mask;

  for (keycode = xkb->min_key_code; keycode <= xkb->max_key_code; keycode++)
    {
      total_syms = XkbKeyNumSyms (xkb, keycode);

      entry = XkbKeySymsPtr (xkb, keycode);
      mask = 0;
      for (i = 0; i < total_syms; i++)
	{
	  switch (entry[i]) {
	  case GDK_KEY_Meta_L:
	  case GDK_KEY_Meta_R:
	    mask |= GDK_META_MASK;
	    break;
	  case GDK_KEY_Hyper_L:
	  case GDK_KEY_Hyper_R:
	    mask |= GDK_HYPER_MASK;
	    break;
	  case GDK_KEY_Super_L:
	  case GDK_KEY_Super_R:
	    mask |= GDK_SUPER_MASK;
	    break;
	  }
	}

      modifier = g_bit_nth_lsf(xkb->map->modmap[keycode], -1);
      keymap->modmap[modifier] |= mask;
    }
}

GdkKeymap *
_gdk_wayland_keymap_new (GdkDisplay *display)
{
  GdkWaylandKeymap *keymap;
  struct xkb_rule_names names;

  keymap = g_object_new (_gdk_wayland_keymap_get_type(), NULL);
  GDK_KEYMAP (keymap)->display = display;

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";
  keymap->xkb = xkb_compile_keymap_from_rules(&names);

  update_modmap (keymap);
  update_keymaps (keymap);

  return GDK_KEYMAP (keymap);
}

struct xkb_desc *_gdk_wayland_keymap_get_xkb_desc (GdkKeymap *keymap)
{
  return GDK_WAYLAND_KEYMAP (keymap)->xkb;
}
