/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gdk/gdk.h>

#include "gdkmacoskeymap-private.h"

G_DEFINE_TYPE (GdkMacosKeymap, gdk_macos_keymap, GDK_TYPE_KEYMAP)

static void
gdk_macos_keymap_finalize (GObject *object)
{
  GdkMacosKeymap *self = (GdkMacosKeymap *)object;

  G_OBJECT_CLASS (gdk_macos_keymap_parent_class)->finalize (object);
}

static void
gdk_macos_keymap_class_init (GdkMacosKeymapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_macos_keymap_finalize;
}

static void
gdk_macos_keymap_init (GdkMacosKeymap *self)
{
}

GdkMacosKeymap *
_gdk_macos_keymap_new (GdkMacosDisplay *display)
{
  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (display), NULL);

  return g_object_new (GDK_TYPE_MACOS_KEYMAP,
                       "display", display,
                       NULL);
}
