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
};

struct _GdkWaylandKeymapClass
{
  GdkKeymapClass parent_class;
};

#define GDK_TYPE_WAYLAND_KEYMAP          (_gdk_wayland_keymap_get_type ())
#define GDK_WAYLAND_KEYMAP(object)       (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_KEYMAP, GdkWaylandKeyMap))
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
  return FALSE;
}

static gboolean
gdk_wayland_keymap_get_entries_for_keycode (GdkKeymap     *keymap,
					guint          hardware_keycode,
					GdkKeymapKey **keys,
					guint        **keyvals,
					gint          *n_entries)
{
  return FALSE;
}

static guint
gdk_wayland_keymap_lookup_key (GdkKeymap          *keymap,
			   const GdkKeymapKey *key)
{
  return 0;
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
  return FALSE;
}


static void
gdk_wayland_keymap_add_virtual_modifiers (GdkKeymap       *keymap,
				      GdkModifierType *state)
{
}

static gboolean
gdk_wayland_keymap_map_virtual_modifiers (GdkKeymap       *keymap,
				      GdkModifierType *state)
{
  return FALSE;
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
