/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc
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

#ifndef __GDK_KEYS_PRIVATE_H__
#define __GDK_KEYS_PRIVATE_H__

#include "gdkkeys.h"

G_BEGIN_DECLS

#define GDK_TYPE_KEYMAP              (gdk_keymap_get_type ())
#define GDK_KEYMAP(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_KEYMAP, GdkKeymap))
#define GDK_IS_KEYMAP(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_KEYMAP))
#define GDK_KEYMAP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_KEYMAP, GdkKeymapClass))
#define GDK_IS_KEYMAP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_KEYMAP))
#define GDK_KEYMAP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_KEYMAP, GdkKeymapClass))

typedef struct _GdkKeymap             GdkKeymap;
typedef struct _GdkKeymapClass GdkKeymapClass;

struct _GdkKeymapClass
{
  GObjectClass parent_class;

  PangoDirection (* get_direction)      (GdkKeymap *keymap);
  gboolean (* have_bidi_layouts)        (GdkKeymap *keymap);
  gboolean (* get_caps_lock_state)      (GdkKeymap *keymap);
  gboolean (* get_num_lock_state)       (GdkKeymap *keymap);
  gboolean (* get_scroll_lock_state)    (GdkKeymap *keymap);
  gboolean (* get_entries_for_keyval)   (GdkKeymap     *keymap,
                                         guint          keyval,
                                         GdkKeymapKey **keys,
                                         gint          *n_keys);
  gboolean (* get_entries_for_keycode)  (GdkKeymap     *keymap,
                                         guint          hardware_keycode,
                                         GdkKeymapKey **keys,
                                         guint        **keyvals,
                                         gint          *n_entries);
  guint (* lookup_key)                  (GdkKeymap          *keymap,
                                         const GdkKeymapKey *key);
  gboolean (* translate_keyboard_state) (GdkKeymap       *keymap,
                                         guint            hardware_keycode,
                                         GdkModifierType  state,
                                         gint             group,
                                         guint           *keyval,
                                         gint            *effective_group,
                                         gint            *level,
                                         GdkModifierType *consumed_modifiers);
  guint (* get_modifier_state)          (GdkKeymap *keymap);


  /* Signals */
  void (*direction_changed) (GdkKeymap *keymap);
  void (*keys_changed)      (GdkKeymap *keymap);
  void (*state_changed)     (GdkKeymap *keymap);
};

struct _GdkKeymap
{
  GObject     parent_instance;
  GdkDisplay *display;
};

GType gdk_keymap_get_type (void) G_GNUC_CONST;

GdkDisplay *   gdk_keymap_get_display              (GdkKeymap           *keymap);

guint          gdk_keymap_lookup_key               (GdkKeymap           *keymap,
                                                    const GdkKeymapKey  *key);
gboolean       gdk_keymap_translate_keyboard_state (GdkKeymap           *keymap,
                                                    guint                hardware_keycode,
                                                    GdkModifierType      state,
                                                    gint                 group,
                                                    guint               *keyval,
                                                    gint                *effective_group,
                                                    gint                *level,
                                                    GdkModifierType     *consumed_modifiers);
gboolean       gdk_keymap_get_entries_for_keyval   (GdkKeymap           *keymap,
                                                    guint                keyval,
                                                    GdkKeymapKey       **keys,
                                                    gint                *n_keys);
gboolean       gdk_keymap_get_entries_for_keycode  (GdkKeymap           *keymap,
                                                    guint                hardware_keycode,
                                                    GdkKeymapKey       **keys,
                                                    guint              **keyvals,
                                                    gint                *n_entries);

PangoDirection gdk_keymap_get_direction            (GdkKeymap           *keymap);
gboolean       gdk_keymap_have_bidi_layouts        (GdkKeymap           *keymap);
gboolean       gdk_keymap_get_caps_lock_state      (GdkKeymap           *keymap);
gboolean       gdk_keymap_get_num_lock_state       (GdkKeymap           *keymap);
gboolean       gdk_keymap_get_scroll_lock_state    (GdkKeymap           *keymap);
guint          gdk_keymap_get_modifier_state       (GdkKeymap           *keymap);

G_END_DECLS

#endif
