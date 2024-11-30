/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <xkbcommon/xkbcommon.h>

#include "gdkkeysprivate.h"
#include "gdkdrmkeymap-private.h"

typedef struct _GdkDrmKeymapPrivate GdkDrmKeymapPrivate;

struct _GdkDrmKeymap
{
  GdkKeymap parent_instance;
};

struct _GdkDrmKeymapClass
{
  GdkKeymapClass parent_class;
};

struct _GdkDrmKeymapPrivate
{
  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
};

G_DEFINE_TYPE_WITH_PRIVATE (GdkDrmKeymap, gdk_drm_keymap, GDK_TYPE_KEYMAP)

static xkb_mod_mask_t
get_xkb_modifiers (struct xkb_keymap *xkb_keymap,
                  GdkModifierType    state)
{
  xkb_mod_mask_t mods = 0;

  if (state & GDK_SHIFT_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT);
  if (state & GDK_LOCK_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS);
  if (state & GDK_CONTROL_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL);
  if (state & GDK_ALT_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT);
  if (state & GDK_SUPER_MASK)
    {
      mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Super");
      mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO);
    }
  if (state & GDK_HYPER_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Hyper");
  if (state & GDK_META_MASK)
    mods |= 1 << xkb_keymap_mod_get_index (xkb_keymap, "Meta");

  return mods;
}

static GdkModifierType
get_gdk_modifiers (struct xkb_keymap *xkb_keymap,
                   xkb_mod_mask_t    mods)
{
  GdkModifierType state = 0;

  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_SHIFT)))
    state |= GDK_SHIFT_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CAPS)))
    state |= GDK_LOCK_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_CTRL)))
    state |= GDK_CONTROL_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_ALT)))
    state |= GDK_ALT_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, XKB_MOD_NAME_LOGO)))
    state |= GDK_SUPER_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Super")))
    state |= GDK_SUPER_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Hyper")))
    state |= GDK_HYPER_MASK;
  if (mods & (1 << xkb_keymap_mod_get_index (xkb_keymap, "Meta")))
    state |= GDK_META_MASK;

  return state;
}

static guint
gdk_drm_keymap_lookup_key (GdkKeymap          *keymap,
                          const GdkKeymapKey *key)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  const xkb_keysym_t *syms;
  int n;

  n = xkb_keymap_key_get_syms_by_level (priv->xkb_keymap,
                                        key->keycode,
                                        key->group,
                                        key->level,
                                        &syms);
  return (n > 0) ? syms[0] : XKB_KEY_NoSymbol;
}

static gboolean
gdk_drm_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                         guint            hardware_keycode,
                                         GdkModifierType  state,
                                         int              group,
                                         guint           *keyval,
                                         int             *effective_group,
                                         int             *level,
                                         GdkModifierType *consumed_modifiers)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  xkb_mod_mask_t modifiers, consumed;
  xkb_layout_index_t layout;
  xkb_level_index_t lvl;
  xkb_keysym_t sym;

  modifiers = get_xkb_modifiers (priv->xkb_keymap, state);
  layout = xkb_state_key_get_layout (priv->xkb_state, hardware_keycode);
  lvl = xkb_state_key_get_level (priv->xkb_state, hardware_keycode, layout);
  sym = xkb_state_key_get_one_sym (priv->xkb_state, hardware_keycode);
  consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (priv->xkb_state,
                                                             hardware_keycode,
                                                             modifiers);

  if (keyval)
    *keyval = sym;
  if (effective_group)
    *effective_group = layout;
  if (level)
    *level = lvl;
  if (consumed_modifiers)
    *consumed_modifiers = get_gdk_modifiers (priv->xkb_keymap, consumed);

  return (sym != XKB_KEY_NoSymbol);
}

static PangoDirection
gdk_drm_keymap_get_direction (GdkKeymap *keymap)
{
  return PANGO_DIRECTION_NEUTRAL;
}

static gboolean
gdk_drm_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_drm_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  return xkb_state_led_name_is_active (priv->xkb_state, XKB_LED_NAME_CAPS);
}

static gboolean
gdk_drm_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  return xkb_state_led_name_is_active (priv->xkb_state, XKB_LED_NAME_NUM);
}

static gboolean
gdk_drm_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  return xkb_state_led_name_is_active (priv->xkb_state, XKB_LED_NAME_SCROLL);
}

static gboolean
gdk_drm_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                        guint          hardware_keycode,
                                        GdkKeymapKey **keys,
                                        guint        **keyvals,
                                        int           *n_entries)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  int num_layouts, layout, num_levels, level;
  GArray *key_entries;
  guint *keyval_entries;
  int n = 0;

  num_layouts = xkb_keymap_num_layouts_for_key (priv->xkb_keymap, hardware_keycode);
  key_entries = g_array_new (FALSE, FALSE, sizeof (GdkKeymapKey));
  keyval_entries = g_new (guint, num_layouts * 4);

  for (layout = 0; layout < num_layouts; layout++)
    {
      num_levels = xkb_keymap_num_levels_for_key (priv->xkb_keymap, hardware_keycode, layout);
      for (level = 0; level < num_levels; level++)
        {
          const xkb_keysym_t *syms;
          int num_syms = xkb_keymap_key_get_syms_by_level (priv->xkb_keymap,
                                                          hardware_keycode,
                                                          layout,
                                                          level,
                                                          &syms);
          if (num_syms > 0)
            {
              GdkKeymapKey k = {
                .keycode = hardware_keycode,
                .group = layout,
                .level = level,
              };
              g_array_append_val (key_entries, k);
              keyval_entries[n++] = syms[0];
            }
        }
    }

  *keys = (GdkKeymapKey *) g_array_free (key_entries, FALSE);
  *keyvals = keyval_entries;
  *n_entries = n;
  return (n > 0);
}

static guint
gdk_drm_keymap_get_modifier_state (GdkKeymap *keymap)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  xkb_mod_mask_t mods = xkb_state_serialize_mods (priv->xkb_state, XKB_STATE_MODS_EFFECTIVE);

  return get_gdk_modifiers (priv->xkb_keymap, mods);
}

static gboolean
gdk_drm_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                       guint      keyval,
                                       GArray    *keys)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  xkb_keycode_t min_keycode = xkb_keymap_min_keycode (priv->xkb_keymap);
  xkb_keycode_t max_keycode = xkb_keymap_max_keycode (priv->xkb_keymap);
  xkb_keycode_t keycode;

  for (keycode = min_keycode; keycode <= max_keycode; keycode++)
    {
      int num_layouts = xkb_keymap_num_layouts_for_key (priv->xkb_keymap, keycode);
      int layout, level;

      for (layout = 0; layout < num_layouts; layout++)
        {
          int num_levels = xkb_keymap_num_levels_for_key (priv->xkb_keymap, keycode, layout);
          for (level = 0; level < num_levels; level++)
            {
              const xkb_keysym_t *syms;
              int num_syms = xkb_keymap_key_get_syms_by_level (priv->xkb_keymap,
                                                              keycode,
                                                              layout,
                                                              level,
                                                              &syms);
              if (num_syms > 0 && syms[0] == (xkb_keysym_t) keyval)
                {
                  GdkKeymapKey k = {
                    .keycode = keycode,
                    .group = layout,
                    .level = level,
                  };
                  g_array_append_val (keys, k);
                }
            }
        }
    }

  return keys->len > 0;
}

static void
gdk_drm_keymap_finalize (GObject *object)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (object));

  xkb_keymap_unref (priv->xkb_keymap);
  xkb_state_unref (priv->xkb_state);

  G_OBJECT_CLASS (gdk_drm_keymap_parent_class)->finalize (object);
}

static void
gdk_drm_keymap_class_init (GdkDrmKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  object_class->finalize = gdk_drm_keymap_finalize;

  keymap_class->get_caps_lock_state = gdk_drm_keymap_get_caps_lock_state;
  keymap_class->get_direction = gdk_drm_keymap_get_direction;
  keymap_class->get_entries_for_keycode = gdk_drm_keymap_get_entries_for_keycode;
  keymap_class->get_entries_for_keyval = gdk_drm_keymap_get_entries_for_keyval;
  keymap_class->get_num_lock_state = gdk_drm_keymap_get_num_lock_state;
  keymap_class->get_scroll_lock_state = gdk_drm_keymap_get_scroll_lock_state;
  keymap_class->have_bidi_layouts = gdk_drm_keymap_have_bidi_layouts;
  keymap_class->lookup_key = gdk_drm_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_drm_keymap_translate_keyboard_state;
  keymap_class->get_modifier_state = gdk_drm_keymap_get_modifier_state;
}

static void
gdk_drm_keymap_init (GdkDrmKeymap *self)
{
}

void
_gdk_drm_keymap_update_key (GdkDrmKeymap *keymap,
                            guint         keycode,
                            gboolean      pressed)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));

  xkb_state_update_key (priv->xkb_state,
                        keycode,
                        pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
}

gboolean
_gdk_drm_keymap_translate_key (GdkDrmKeymap     *keymap,
                               guint             keycode,
                               GdkModifierType   state,
                               GdkTranslatedKey *translated,
                               GdkTranslatedKey *no_lock)
{
  GdkDrmKeymapPrivate *priv = gdk_drm_keymap_get_instance_private (GDK_DRM_KEYMAP (keymap));
  xkb_mod_mask_t modifiers, consumed;
  xkb_layout_index_t layout;
  xkb_level_index_t level;
  xkb_keysym_t sym;
  struct xkb_state *tmp_state;

  if (!translated && !no_lock)
    return FALSE;

  modifiers = get_xkb_modifiers (priv->xkb_keymap, state);

  if (translated)
    {
      layout = xkb_state_key_get_layout (priv->xkb_state, keycode);
      level = xkb_state_key_get_level (priv->xkb_state, keycode, layout);
      sym = xkb_state_key_get_one_sym (priv->xkb_state, keycode);
      consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (priv->xkb_state,
                                                                  keycode,
                                                                  modifiers);
      translated->keyval = sym;
      translated->consumed = get_gdk_modifiers (priv->xkb_keymap, consumed);
      translated->layout = layout;
      translated->level = level;
    }

  if (no_lock)
    {
      tmp_state = xkb_state_new (priv->xkb_keymap);
      xkb_state_update_mask (tmp_state, modifiers, 0, 0, 0, 0, 0);
      layout = xkb_state_key_get_layout (tmp_state, keycode);
      level = xkb_state_key_get_level (tmp_state, keycode, layout);
      sym = xkb_state_key_get_one_sym (tmp_state, keycode);
      consumed = modifiers & ~xkb_state_mod_mask_remove_consumed (tmp_state,
                                                                  keycode,
                                                                  modifiers);
      no_lock->keyval = sym;
      no_lock->consumed = get_gdk_modifiers (priv->xkb_keymap, consumed);
      no_lock->layout = layout;
      no_lock->level = level;
      xkb_state_unref (tmp_state);
    }

  return TRUE;
}

GdkDrmKeymap *
_gdk_drm_keymap_new (GdkDrmDisplay *display)
{
  GdkDrmKeymap *keymap;
  GdkDrmKeymapPrivate *priv;
  struct xkb_context *context;
  struct xkb_rule_names names = {
    .rules = "evdev",
    .model = "pc105",
    .layout = "us",
    .variant = "",
    .options = "",
  };

  keymap = g_object_new (GDK_TYPE_DRM_KEYMAP, NULL);
  GDK_KEYMAP (keymap)->display = GDK_DISPLAY (display);

  priv = gdk_drm_keymap_get_instance_private (keymap);
  context = xkb_context_new (XKB_CONTEXT_NO_FLAGS);
  priv->xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
  priv->xkb_state = xkb_state_new (priv->xkb_keymap);
  xkb_context_unref (context);

  return keymap;
}
