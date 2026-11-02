/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2026 Red Hat, Inc.
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

#include "config.h"

#include <stdint.h>
#include <glib.h>
#include <windows.h>

/* Windows-native counterpart to gdk/wayland/gdkfractionalscale-private.h: instead of
 * Wayland's abstract 120-based numerator, Windows hands us a real DPI directly in every
 * message, exact by construction, so we store that and derive the double scale (dpi / 96)
 * only when needed. */
#define GDK_WIN32_DEFAULT_DPI 96 /* == USER_DEFAULT_SCREEN_DPI */

typedef struct _GdkWin32Dpi GdkWin32Dpi;

struct _GdkWin32Dpi
{
  uint32_t dpi;
};

#define GDK_WIN32_DPI_INIT(dpi) (GdkWin32Dpi) { dpi }

/* this DPI's scale factor, for the portable gdk_surface_get_scale() boundary */
static inline double
gdk_win32_dpi_to_scale (const GdkWin32Dpi *self)
{
  return (double) self->dpi / GDK_WIN32_DEFAULT_DPI;
}

static inline gboolean
gdk_win32_dpi_equal (const GdkWin32Dpi *a,
                     const GdkWin32Dpi *b)
{
  return a->dpi == b->dpi;
}

/* application -> device, rounding to nearest (MulDiv ties away from zero) */
static inline int
gdk_win32_dpi_to_device (const GdkWin32Dpi *self,
                         int                value)
{
  return MulDiv (value, (int) self->dpi, GDK_WIN32_DEFAULT_DPI);
}

/* device -> application, rounding to nearest */
static inline int
gdk_win32_dpi_to_application (const GdkWin32Dpi *self,
                              int                value)
{
  return MulDiv (value, GDK_WIN32_DEFAULT_DPI, (int) self->dpi);
}

/* device -> application, preserving sub-pixel precision (input/event
 * coordinates are application doubles by API contract). */
static inline double
gdk_win32_dpi_to_application_double (const GdkWin32Dpi *self,
                                     double             value)
{
  return value * GDK_WIN32_DEFAULT_DPI / self->dpi;
}

/* device -> application, rounding up: the smallest application extent that still
 * covers `value` device pixels (never a sub-pixel gap at fractional scale) */
static inline int
gdk_win32_dpi_to_application_ceil (const GdkWin32Dpi *self,
                                   int                value)
{
  return (value * GDK_WIN32_DEFAULT_DPI + (int) self->dpi - 1) / (int) self->dpi;
}
