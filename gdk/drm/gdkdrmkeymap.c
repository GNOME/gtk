/* gdkdrmkeymap.c
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <xkbcommon/xkbcommon.h>

#include <gdk/gdk.h>

#include "gdkkeysprivate.h"
#include "gdkkeysyms.h"
#include "gdkdrmkeymap-private.h"

struct _GdkDrmKeymap
{
  GdkKeymap parent_instance;
};

struct _GdkDrmKeymapClass
{
  GdkKeymapClass parent_instance;
};

G_DEFINE_TYPE (GdkDrmKeymap, gdk_drm_keymap, GDK_TYPE_KEYMAP)

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
  return FALSE;
}

static gboolean
gdk_drm_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static gboolean
gdk_drm_keymap_get_scroll_lock_state (GdkKeymap *keymap)
{
  return FALSE;
}

static guint
gdk_drm_keymap_lookup_key (GdkKeymap          *keymap,
                           const GdkKeymapKey *key)
{
  GdkDrmKeymap *self = (GdkDrmKeymap *)keymap;

  g_assert (GDK_IS_DRM_KEYMAP (self));
  g_assert (key != NULL);

  /* TODO */

  return XKB_KEY_NoSymbol;
}

static gboolean
gdk_drm_keymap_get_entries_for_keyval (GdkKeymap *keymap,
                                       guint      keyval,
                                       GArray    *keys)
{
  g_assert (GDK_IS_DRM_KEYMAP (keymap));
  g_assert (keys != NULL);

  return FALSE;
}

static gboolean
gdk_drm_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                          guint          hardware_keycode,
                                          GdkKeymapKey **keys,
                                          guint        **keyvals,
                                          int           *n_entries)
{
  g_assert (GDK_IS_DRM_KEYMAP (keymap));
  g_assert (keyvals != NULL);
  g_assert (n_entries != NULL);

  *keyvals = NULL;
  *n_entries = 0;

  return FALSE;
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
  g_assert (GDK_IS_DRM_KEYMAP (keymap));

  if (keyval)
    *keyval = 0;

  if (effective_group)
    *effective_group = 0;

  if (level)
    *level = 0;

  if (consumed_modifiers)
    *consumed_modifiers = 0;

  return FALSE;
}

static void
gdk_drm_keymap_finalize (GObject *object)
{
  G_OBJECT_CLASS (gdk_drm_keymap_parent_class)->finalize (object);
}

static void
gdk_drm_keymap_class_init (GdkDrmKeymapClass *klass)
{
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
}

static void
gdk_drm_keymap_init (GdkDrmKeymap *self)
{
}

GdkDrmKeymap *
_gdk_drm_keymap_new (GdkDrmDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DRM_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_DRM_KEYMAP,
                       "display", display,
                       NULL);
}
