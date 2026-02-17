/* gdkdrmkeymap-private.h
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

#pragma once

#include "gdkdrmdisplay.h"
#include "gdkdrmkeymap.h"

#include "gdkeventsprivate.h"

G_BEGIN_DECLS

GdkDrmKeymap *_gdk_drm_keymap_new (GdkDrmDisplay *display);

void          _gdk_drm_keymap_update_key (GdkDrmKeymap   *keymap,
                                         guint           keycode,
                                         gboolean        pressed);
gboolean      _gdk_drm_keymap_translate_key (GdkDrmKeymap     *keymap,
                                             guint             keycode,
                                             GdkModifierType   state,
                                             GdkTranslatedKey *translated,
                                             GdkTranslatedKey *no_lock);

G_END_DECLS
