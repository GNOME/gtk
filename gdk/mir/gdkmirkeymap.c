/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <xkbcommon/xkbcommon.h>

#include "gdkkeysprivate.h"

typedef struct GdkMirKeymap      GdkMirKeymap;
typedef struct GdkMirKeymapClass GdkMirKeymapClass;

#define GDK_TYPE_MIR_KEYMAP              (gdk_mir_keymap_get_type ())
#define GDK_MIR_KEYMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_KEYMAP, GdkMirKeymap))
#define GDK_MIR_KEYMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_KEYMAP, GdkMirKeymapClass))
#define GDK_IS_MIR_KEYMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_KEYMAP))
#define GDK_IS_MIR_KEYMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_KEYMAP))
#define GDK_MIR_KEYMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_KEYMAP, GdkMirKeymapClass))

#define IsModifierKey(keysym) \
  (((keysym) >= XKB_KEY_Shift_L && (keysym) <= XKB_KEY_Hyper_R) || \
   ((keysym) >= XKB_KEY_ISO_Lock && (keysym) <= XKB_KEY_ISO_Last_Group_Lock) || \
   ((keysym) == XKB_KEY_Mode_switch) || \
   ((keysym) == XKB_KEY_Num_Lock))

struct GdkMirKeymap
{
  GdkKeymap parent_instance;

  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;

  PangoDirection *direction;
  gboolean bidi;
};

struct GdkMirKeymapClass
{
  GdkKeymapClass parent_class;
};

G_DEFINE_TYPE (GdkMirKeymap, gdk_mir_keymap, GDK_TYPE_KEYMAP)

GdkKeymap *
_gdk_mir_keymap_new (void)
{
  return g_object_new (GDK_TYPE_MIR_KEYMAP, NULL);
}

static PangoDirection
gdk_mir_keymap_get_direction (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_get_direction\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  gint i;

  for (i = 0; i < xkb_keymap_num_layouts (mir_keymap->xkb_keymap); i++)
    {
      if (xkb_state_layout_index_is_active (mir_keymap->xkb_state, i, XKB_STATE_LAYOUT_EFFECTIVE))
        return mir_keymap->direction[i];
    }

  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_mir_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_have_bidi_layouts\n");
  return FALSE;
}

static gboolean
gdk_mir_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_get_caps_lock_state\n");
  return xkb_state_led_name_is_active (GDK_MIR_KEYMAP (keymap)->xkb_state, XKB_LED_NAME_CAPS);
}

static gboolean
gdk_mir_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_get_num_lock_state\n");
  return xkb_state_led_name_is_active (GDK_MIR_KEYMAP (keymap)->xkb_state, XKB_LED_NAME_NUM);
}

static gboolean
gdk_mir_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_get_scroll_lock_state\n");
  return xkb_state_led_name_is_active (GDK_MIR_KEYMAP (keymap)->xkb_state, XKB_LED_NAME_SCROLL);
}

static gboolean
gdk_mir_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                       guint          keyval,
                                       GdkKeymapKey **keys,
                                       gint          *n_keys)
{
  //g_printerr ("gdk_mir_keymap_get_entries_for_keyval\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  GArray *key_array;
  guint keycode;

  key_array = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));

  for (keycode = 8; keycode < 255; keycode++) /* FIXME: min/max keycode */
    {
      gint num_layouts, layout;

      num_layouts = xkb_keymap_num_layouts_for_key (mir_keymap->xkb_keymap, keycode);
      for (layout = 0; layout < num_layouts; layout++)
        {
          gint num_levels, level;

          num_levels = xkb_keymap_num_levels_for_key (mir_keymap->xkb_keymap, keycode, layout);
          for (level = 0; level < num_levels; level++)
            {
              const xkb_keysym_t *syms;
              gint num_syms, sym;

              num_syms = xkb_keymap_key_get_syms_by_level (mir_keymap->xkb_keymap, keycode, layout, level, &syms);
              for (sym = 0; sym < num_syms; sym++)
                {
                  if (syms[sym] == keyval)
                    {
                      GdkKeymapKey key;

                      key.keycode = keycode;
                      key.group = layout;
                      key.level = level;

                      g_array_append_val (key_array, key);
                    }
                }
            }
        }
    }

  *n_keys = key_array->len;
  *keys = (GdkKeymapKey*) g_array_free (key_array, FALSE);

  return TRUE;
}

static gboolean
gdk_mir_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                        guint          hardware_keycode,
                                        GdkKeymapKey **keys,
                                        guint        **keyvals,
                                        gint          *n_entries)
{
  //g_printerr ("gdk_mir_keymap_get_entries_for_keycode\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  gint num_layouts, layout;
  gint num_entries;
  gint i;

  num_layouts = xkb_keymap_num_layouts_for_key (mir_keymap->xkb_keymap, hardware_keycode);

  num_entries = 0;
  for (layout = 0; layout < num_layouts; layout++)
    num_entries += xkb_keymap_num_levels_for_key (mir_keymap->xkb_keymap, hardware_keycode,  layout);

 if (n_entries)
    *n_entries = num_entries;
  if (keys)
    *keys = g_new0 (GdkKeymapKey, num_entries);
  if (keyvals)
    *keyvals = g_new0 (guint, num_entries);

  i = 0;
  for (layout = 0; layout < num_layouts; layout++)
    {
      gint num_levels, level;
      num_levels = xkb_keymap_num_levels_for_key (mir_keymap->xkb_keymap, hardware_keycode, layout);
      for (level = 0; level < num_levels; level++)
        {
          const xkb_keysym_t *syms;
          int num_syms;

          num_syms = xkb_keymap_key_get_syms_by_level (mir_keymap->xkb_keymap, hardware_keycode, layout, 0, &syms);
          if (keys)
            {
              (*keys)[i].keycode = hardware_keycode;
              (*keys)[i].group = layout;
              (*keys)[i].level = level;
            }
          if (keyvals && num_syms > 0)
            (*keyvals)[i] = syms[0];

          i++;
        }
    }

  return num_entries > 0;
}

static guint
gdk_mir_keymap_lookup_key (GdkKeymap          *keymap,
                           const GdkKeymapKey *key)
{
  //g_printerr ("gdk_mir_keymap_lookup_key\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  const xkb_keysym_t *syms;
  int num_syms;

  num_syms = xkb_keymap_key_get_syms_by_level (mir_keymap->xkb_keymap,
                                               key->keycode,
                                               key->group,
                                               key->level,
                                               &syms);
  if (num_syms > 0)
    return syms[0];
  else
    return XKB_KEY_NoSymbol;
}

static guint32
get_xkb_modifiers (struct xkb_keymap *xkb_keymap,
                   GdkModifierType    state)
{
  guint32 mods = 0;

  if (state & GDK_SHIFT_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT);
  if (state & GDK_LOCK_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS);
  if (state & GDK_CONTROL_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL);
  if (state & GDK_MOD1_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT);
  if (state & GDK_MOD2_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod2");
  if (state & GDK_MOD3_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod3");
  if (state & GDK_MOD4_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO);
  if (state & GDK_MOD5_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod5");

  return mods;
}

static GdkModifierType
get_gdk_modifiers (struct xkb_keymap *xkb_keymap,
                   guint32            mods)
{
  GdkModifierType state = 0;

  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT)))
    state |= GDK_SHIFT_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS)))
    state |= GDK_LOCK_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL)))
    state |= GDK_CONTROL_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT)))
    state |= GDK_MOD1_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod2")))
    state |= GDK_MOD2_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod3")))
    state |= GDK_MOD3_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO)))
    state |= GDK_MOD4_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Mod5")))
    state |= GDK_MOD5_MASK;

  return state;
}

static gboolean
gdk_mir_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                         guint            hardware_keycode,
                                         GdkModifierType  state,
                                         gint             group,
                                         guint           *keyval,
                                         gint            *effective_group,
                                         gint            *effective_level,
                                         GdkModifierType *consumed_modifiers)
{
  //g_printerr ("gdk_mir_keymap_translate_keyboard_state\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  struct xkb_state *xkb_state;
  guint32 modifiers;
  guint32 consumed;
  xkb_layout_index_t layout;
  xkb_level_index_t level;
  xkb_keysym_t sym;

  modifiers = get_xkb_modifiers (mir_keymap->xkb_keymap, state);

  xkb_state = xkb_state_new (mir_keymap->xkb_keymap);

  xkb_state_update_mask (xkb_state, modifiers, 0, 0, group, 0, 0);

  layout = xkb_state_key_get_layout (xkb_state, hardware_keycode);
  level = xkb_state_key_get_level (xkb_state, hardware_keycode, layout);
  sym = xkb_state_key_get_one_sym (xkb_state, hardware_keycode);
  consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (xkb_state, hardware_keycode, modifiers);

  xkb_state_unref (xkb_state);

  if (keyval)
    *keyval = sym;
  if (effective_group)
    *effective_group = layout;
  if (effective_level)
    *effective_level = level;
  if (consumed_modifiers)
    *consumed_modifiers = get_gdk_modifiers (mir_keymap->xkb_keymap, consumed);

  return TRUE;
}

static void
gdk_mir_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
                                      GdkModifierType *state)
{
  //g_printerr ("gdk_mir_keymap_add_virtual_modifiers\n");
  // FIXME: What is this?
}

static gboolean
gdk_mir_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                      GdkModifierType *state)
{
  //g_printerr ("gdk_mir_keymap_map_virtual_modifiers\n");
  // FIXME: What is this?
  return TRUE;
}

static guint
gdk_mir_keymap_get_modifier_state (GdkKeymap *keymap)
{
  //g_printerr ("gdk_mir_keymap_get_modifier_state\n");
  GdkMirKeymap *mir_keymap = GDK_MIR_KEYMAP (keymap);
  xkb_mod_mask_t mods;

  mods = xkb_state_serialize_mods (mir_keymap->xkb_state, XKB_STATE_MODS_EFFECTIVE);

  return get_gdk_modifiers (mir_keymap->xkb_keymap, mods);
}

gboolean
_gdk_mir_keymap_key_is_modifier (GdkKeymap *keymap,
                                 guint      keycode)
{
  // FIXME: use xkb_state
  return IsModifierKey (keycode);
}

static void
update_direction (GdkMirKeymap *keymap)
{
  gint num_layouts;
  gint *rtl;
  guint key;
  gboolean have_rtl, have_ltr;
  gint i;

  num_layouts = xkb_keymap_num_layouts (keymap->xkb_keymap);

  g_free (keymap->direction);
  keymap->direction = g_new0 (PangoDirection, num_layouts);

  rtl = g_new0 (gint, num_layouts);

  for (key = 8; key < 255; key++) /* FIXME: min/max keycode */
    {
       gint layouts;
       gint layout;

       layouts = xkb_keymap_num_layouts_for_key (keymap->xkb_keymap, key);
       for (layout = 0; layout < layouts; layout++)
         {
           const xkb_keysym_t *syms;
           gint num_syms;
           gint sym;

           num_syms = xkb_keymap_key_get_syms_by_level (keymap->xkb_keymap, key, layout, 0, &syms);
           for (sym = 0; sym < num_syms; sym++)
             {
               PangoDirection dir;
               dir = pango_unichar_direction (xkb_keysym_to_utf32 (syms[sym]));
               switch (dir)
                 {
                 case PANGO_DIRECTION_RTL:
                   rtl[layout]++;
                   break;
                 case PANGO_DIRECTION_LTR:
                   rtl[layout]--;
                   break;
                 default:
                   break;
                 }
             }
         }
    }

  have_rtl = have_ltr = FALSE;
  for (i = 0; i < num_layouts; i++)
    {
      if (rtl[i] > 0)
        {
          keymap->direction[i] = PANGO_DIRECTION_RTL;
          have_rtl = TRUE;
        }
      else
        {
          keymap->direction[i] = PANGO_DIRECTION_LTR;
          have_ltr = TRUE;
        }
    }

  if (have_rtl && have_ltr)
    keymap->bidi = TRUE;

  g_free (rtl);
}

static void
gdk_mir_keymap_init (GdkMirKeymap *keymap)
{
  struct xkb_context *context;
  struct xkb_rule_names names;

  context = xkb_context_new (0);

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";
  keymap->xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
  keymap->xkb_state = xkb_state_new (keymap->xkb_keymap);

  xkb_context_unref (context);

  update_direction (keymap);
}

static void
gdk_mir_keymap_finalize (GObject *object)
{
  GdkMirKeymap *keymap = GDK_MIR_KEYMAP (object);

  xkb_keymap_unref (keymap->xkb_keymap);
  xkb_state_unref (keymap->xkb_state);
  g_free (keymap->direction);

  G_OBJECT_CLASS (gdk_mir_keymap_parent_class)->finalize (object);
}

static void
gdk_mir_keymap_class_init (GdkMirKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_mir_keymap_finalize;

  keymap_class->get_direction = gdk_mir_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_mir_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_mir_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_mir_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_mir_keymap_get_scroll_lock_state;
  keymap_class->get_entries_for_keyval = gdk_mir_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_mir_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_mir_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_mir_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_mir_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_mir_keymap_map_virtual_modifiers;
  keymap_class->get_modifier_state = gdk_mir_keymap_get_modifier_state;
}
