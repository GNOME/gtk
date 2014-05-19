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

#include "gdkkeysprivate.h"

typedef struct GdkMirKeymap      GdkMirKeymap;
typedef struct GdkMirKeymapClass GdkMirKeymapClass;

#define GDK_TYPE_MIR_KEYMAP              (gdk_mir_keymap_get_type ())
#define GDK_MIR_KEYMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_KEYMAP, GdkMirKeymap))
#define GDK_MIR_KEYMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_KEYMAP, GdkMirKeymapClass))
#define GDK_IS_MIR_KEYMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_KEYMAP))
#define GDK_IS_MIR_KEYMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_KEYMAP))
#define GDK_MIR_KEYMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_KEYMAP, GdkMirKeymapClass))

struct GdkMirKeymap
{
  GdkKeymap parent_instance;
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
  g_printerr ("gdk_mir_keymap_get_direction\n");
  return PANGO_DIRECTION_LTR;
}

static gboolean
gdk_mir_keymap_have_bidi_layouts (GdkKeymap *keymap)
{
  g_printerr ("gdk_mir_keymap_have_bidi_layouts\n");
  return FALSE;
}

static gboolean
gdk_mir_keymap_get_caps_lock_state (GdkKeymap *keymap)
{
  g_printerr ("gdk_mir_keymap_get_caps_lock_state\n");
  return FALSE; // FIXME
}

static gboolean
gdk_mir_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  g_printerr ("gdk_mir_keymap_get_num_lock_state\n");
  return FALSE; // FIXME
}

static gboolean
gdk_mir_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
                                       guint          keyval,
                                       GdkKeymapKey **keys,
                                       gint          *n_keys)
{
  g_printerr ("gdk_mir_keymap_get_entries_for_keyval\n");
  return FALSE; // FIXME
}

static gboolean
gdk_mir_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
                                        guint          hardware_keycode,
                                        GdkKeymapKey **keys,
                                        guint        **keyvals,
                                        gint          *n_entries)
{
  g_printerr ("gdk_mir_keymap_get_entries_for_keycode\n");
  return FALSE; // FIXME
}

static guint
gdk_mir_keymap_lookup_key (GdkKeymap          *keymap,
                           const GdkKeymapKey *key)
{
  g_printerr ("gdk_mir_keymap_lookup_key\n");
  return 0; // FIXME
}

static gboolean
gdk_mir_keymap_translate_keyboard_state (GdkKeymap       *keymap,
                                         guint            hardware_keycode,
                                         GdkModifierType  state,
                                         gint             group,
                                         guint           *keyval,
                                         gint            *effective_group,
                                         gint            *level,
                                         GdkModifierType *consumed_modifiers)
{
  g_printerr ("gdk_mir_keymap_translate_keyboard_state\n");
  return FALSE; // FIXME
}

static void
gdk_mir_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
                                      GdkModifierType *state)
{
  g_printerr ("gdk_mir_keymap_add_virtual_modifiers\n");
  // FIXME
}

static gboolean
gdk_mir_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
                                      GdkModifierType *state)
{
  g_printerr ("gdk_mir_keymap_map_virtual_modifiers\n");
  return FALSE; // FIXME
}

static GdkModifierType
gdk_mir_keymap_get_modifier_mask (GdkKeymap         *keymap,
                                  GdkModifierIntent  intent)
{
  g_printerr ("gdk_mir_keymap_get_modifier_mask\n");
  return 0; // FIXME GDK_MODIFIER_TYPE_
}

static guint
gdk_mir_keymap_get_modifier_state (GdkKeymap *keymap)
{
  g_printerr ("gdk_mir_keymap_get_modifier_state\n");
  return 0; // FIXME
}

static void
gdk_mir_keymap_init (GdkMirKeymap *keymap)
{
}

static void
gdk_mir_keymap_class_init (GdkMirKeymapClass *klass)
{
  GdkKeymapClass *keymap_class = GDK_KEYMAP_CLASS (klass);

  keymap_class->get_direction = gdk_mir_keymap_get_direction;
  keymap_class->have_bidi_layouts = gdk_mir_keymap_have_bidi_layouts;
  keymap_class->get_caps_lock_state = gdk_mir_keymap_get_caps_lock_state;
  keymap_class->get_num_lock_state = gdk_mir_keymap_get_num_lock_state;
  keymap_class->get_entries_for_keyval = gdk_mir_keymap_get_entries_for_keyval;
  keymap_class->get_entries_for_keycode = gdk_mir_keymap_get_entries_for_keycode;
  keymap_class->lookup_key = gdk_mir_keymap_lookup_key;
  keymap_class->translate_keyboard_state = gdk_mir_keymap_translate_keyboard_state;
  keymap_class->add_virtual_modifiers = gdk_mir_keymap_add_virtual_modifiers;
  keymap_class->map_virtual_modifiers = gdk_mir_keymap_map_virtual_modifiers;
  keymap_class->get_modifier_mask = gdk_mir_keymap_get_modifier_mask;
  keymap_class->get_modifier_state = gdk_mir_keymap_get_modifier_state;
}
