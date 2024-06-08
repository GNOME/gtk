/* gdknamedscolorstate.h
 *
 * Copyright 2024 (c) Matthias Clasen
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

#pragma once

#include "gdkcolorstateprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_NAMED_COLOR_STATE (gdk_named_color_state_get_type ())

typedef enum {
  GDK_COLOR_STATE_SRGB,
  GDK_COLOR_STATE_SRGB_LINEAR,
  GDK_COLOR_STATE_HSL,
  GDK_COLOR_STATE_HWB,
  GDK_COLOR_STATE_OKLAB,
  GDK_COLOR_STATE_OKLCH,
} GdkColorStateId;

GDK_DECLARE_INTERNAL_TYPE (GdkNamedColorState, gdk_named_color_state, GDK, NAMED_COLOR_STATE, GdkColorState)

GdkColorStateId gdk_named_color_state_get_id (GdkColorState *self);

G_END_DECLS
