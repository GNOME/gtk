/*
 * Copyright (c) 2026 Kristjan ESPERANTO
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

/* Opaque GSource subtype.  Cast to GSource * for g_source_* calls. */
typedef struct _GdkAndroidChoreographerSource GdkAndroidChoreographerSource;

/*< private >
 * gdk_android_choreographer_source_new:
 * @context: the #GMainContext to attach to
 *
 * Creates a new #GdkAndroidChoreographerSource.  One source is created
 * per display (not per surface), mirroring the per-monitor CVDisplayLink
 * model used by the macOS backend.
 *
 * The source starts in a paused state; call
 * gdk_android_choreographer_source_unpause() when at least one surface
 * becomes visible.
 *
 * Returns: (transfer full): a floating #GSource reference.
 */
GSource *gdk_android_choreographer_source_new     (GMainContext *context);

void     gdk_android_choreographer_source_pause   (GdkAndroidChoreographerSource *source);
void     gdk_android_choreographer_source_unpause (GdkAndroidChoreographerSource *source);

gint64   gdk_android_choreographer_source_get_presentation_time (GdkAndroidChoreographerSource *source);

G_END_DECLS
