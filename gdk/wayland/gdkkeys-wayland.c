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
#include <sys/mman.h>

#include "gdk.h"
#include "gdkwayland.h"

#include "gdkprivate-wayland.h"
#include "gdkinternals.h"
#include "gdkkeysprivate.h"

#include <xkbcommon/xkbcommon.h>

typedef struct _GdkWaylandKeymap          GdkWaylandKeymap;
typedef struct _GdkWaylandKeymapClass     GdkWaylandKeymapClass;

struct _GdkWaylandKeymap
{
  GdkKeymap parent_instance;

  struct xkb_keymap *xkb_keymap;
  struct xkb_state *xkb_state;
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
  return xkb_state_led_name_is_active (GDK_WAYLAND_KEYMAP (keymap)->xkb_state,
                                       XKB_LED_NAME_CAPS);
}

static gboolean
gdk_wayland_keymap_get_num_lock_state (GdkKeymap *keymap)
{
  return xkb_state_led_name_is_active (GDK_WAYLAND_KEYMAP (keymap)->xkb_state,
                                       XKB_LED_NAME_NUM);
}

static gboolean
gdk_wayland_keymap_get_entries_for_keyval (GdkKeymap     *keymap,
					   guint          keyval,
					   GdkKeymapKey **keys,
					   gint          *n_keys)
{
  if (n_keys)
    *n_keys = 1;
  if (keys)
    {
      *keys = g_new0 (GdkKeymapKey, 1);
      (*keys)->keycode = keyval;
    }

  return TRUE;
}

static gboolean
gdk_wayland_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
					    guint          hardware_keycode,
					    GdkKeymapKey **keys,
					    guint        **keyvals,
					    gint          *n_entries)
{
 if (n_entries)
    *n_entries = 1;
  if (keys)
    {
      *keys = g_new0 (GdkKeymapKey, 1);
      (*keys)->keycode = hardware_keycode;
    }
  if (keyvals)
    {
      *keyvals = g_new0 (guint, 1);
      (*keyvals)[0] = hardware_keycode;
    }
  return TRUE;
}

static guint
gdk_wayland_keymap_lookup_key (GdkKeymap          *keymap,
			       const GdkKeymapKey *key)
{
  return key->keycode;
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
  g_return_val_if_fail (keymap == NULL || GDK_IS_KEYMAP (keymap), FALSE);
  g_return_val_if_fail (group < 4, FALSE);

  if (keyval)
    *keyval = hardware_keycode;
  if (effective_group)
    *effective_group = 0;
  if (level)
    *level = 0;
  if (consumed_modifiers)
    *consumed_modifiers = 0;

  return TRUE;
}

static void
gdk_wayland_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
					  GdkModifierType *state)
{
  return;
}

static gboolean
gdk_wayland_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
					  GdkModifierType *state)
{
  return TRUE;
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

GdkKeymap *
_gdk_wayland_keymap_new ()
{
  GdkWaylandKeymap *keymap;
  struct xkb_context *context;
  struct xkb_rule_names names;

  keymap = g_object_new (_gdk_wayland_keymap_get_type(), NULL);

  context = xkb_context_new (0);

  names.rules = "evdev";
  names.model = "pc105";
  names.layout = "us";
  names.variant = "";
  names.options = "";
  keymap->xkb_keymap = xkb_keymap_new_from_names (context, &names, 0);
  keymap->xkb_state = xkb_state_new (keymap->xkb_keymap);
  xkb_context_unref (context);

  return GDK_KEYMAP (keymap);
}

GdkKeymap *
_gdk_wayland_keymap_new_from_fd (uint32_t format,
                                 uint32_t fd, uint32_t size)
{
  GdkWaylandKeymap *keymap;
  struct xkb_context *context;
  char *map_str;

  keymap = g_object_new (_gdk_wayland_keymap_get_type(), NULL);

  context = xkb_context_new (0);

  map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
  if (map_str == MAP_FAILED) {
    close(fd);
    return NULL;
  }

  keymap->xkb_keymap = xkb_keymap_new_from_string (context, map_str, format, 0);
  munmap (map_str, size);
  close (fd);
  keymap->xkb_state = xkb_state_new (keymap->xkb_keymap);
  xkb_context_unref (context);

  return GDK_KEYMAP (keymap);
}

struct xkb_keymap *_gdk_wayland_keymap_get_xkb_keymap (GdkKeymap *keymap)
{
  return GDK_WAYLAND_KEYMAP (keymap)->xkb_keymap;
}

struct xkb_state *_gdk_wayland_keymap_get_xkb_state (GdkKeymap *keymap)
{
  return GDK_WAYLAND_KEYMAP (keymap)->xkb_state;
}
