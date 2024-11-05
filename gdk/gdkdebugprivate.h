/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat, Inc.
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

#include <glib.h>

#include "gdktypes.h"
#include <glib/gstdio.h>

G_BEGIN_DECLS

typedef enum {
  GDK_DEBUG_MISC            = 1 <<  0,
  GDK_DEBUG_EVENTS          = 1 <<  1,
  GDK_DEBUG_DND             = 1 <<  2,
  GDK_DEBUG_INPUT           = 1 <<  3,
  GDK_DEBUG_EVENTLOOP       = 1 <<  4,
  GDK_DEBUG_FRAMES          = 1 <<  5,
  GDK_DEBUG_SETTINGS        = 1 <<  6,
  GDK_DEBUG_OPENGL          = 1 <<  7,
  GDK_DEBUG_VULKAN          = 1 <<  8,
  GDK_DEBUG_SELECTION       = 1 <<  9,
  GDK_DEBUG_CLIPBOARD       = 1 << 10,
  GDK_DEBUG_DMABUF          = 1 << 11,
  GDK_DEBUG_OFFLOAD         = 1 << 12,

  /* flags below are influencing behavior */
  GDK_DEBUG_LINEAR          = 1 << 13,
  GDK_DEBUG_HDR             = 1 << 14,
  GDK_DEBUG_PORTALS         = 1 << 15,
  GDK_DEBUG_NO_PORTALS      = 1 << 16,
  GDK_DEBUG_GL_NO_FRACTIONAL= 1 << 17,
  GDK_DEBUG_FORCE_OFFLOAD   = 1 << 18,
  GDK_DEBUG_GL_PREFER_GL    = 1 << 19,
  GDK_DEBUG_GL_DEBUG        = 1 << 20,
  GDK_DEBUG_DEFAULT_SETTINGS= 1 << 21,
  GDK_DEBUG_HIGH_DEPTH      = 1 << 22,
  GDK_DEBUG_NO_VSYNC        = 1 << 23,
} GdkDebugFlags;

typedef enum {
  GDK_FEATURE_OPENGL           = 1 << 0,
  GDK_FEATURE_GL_API           = 1 << 1,
  GDK_FEATURE_GLES_API         = 1 << 2,
  GDK_FEATURE_EGL              = 1 << 3,
  GDK_FEATURE_GLX              = 1 << 4,
  GDK_FEATURE_WGL              = 1 << 5,
  GDK_FEATURE_VULKAN           = 1 << 6,
  GDK_FEATURE_DMABUF           = 1 << 7,
  GDK_FEATURE_OFFLOAD          = 1 << 8,
  GDK_FEATURE_COLOR_MANAGEMENT = 1 << 9,
  GDK_FEATURE_AEROSNAP         = 1 << 10,
} GdkFeatures;

#define GDK_ALL_FEATURES ((1 << 10) - 1)

extern guint _gdk_debug_flags;

GdkDebugFlags    gdk_display_get_debug_flags    (GdkDisplay       *display);
void             gdk_display_set_debug_flags    (GdkDisplay       *display,
                                                 GdkDebugFlags     flags);

gboolean         gdk_has_feature                (GdkFeatures       feature);

static inline void
gdk_debug_message (const char *format, ...) G_GNUC_PRINTF(1, 2);
static inline void
gdk_debug_message (const char *format, ...)
{
  va_list args;
  char *s;

  va_start (args, format);
  s = g_strdup_vprintf (format, args);
  va_end (args);
#ifdef GLIB_USING_SYSTEM_PRINTF
  fprintf (stderr, "%s\n", s);
#else
  g_fprintf (stderr, "%s\n", s);
#endif
  g_free (s);
}

#define GDK_DISPLAY_DEBUG_CHECK(display,type) \
    G_UNLIKELY (gdk_display_get_debug_flags (display) & GDK_DEBUG_##type)

#define GDK_DISPLAY_DEBUG(display,type,...)                               \
    G_STMT_START {                                                        \
    if (GDK_DISPLAY_DEBUG_CHECK (display,type))                           \
      gdk_debug_message (__VA_ARGS__);                                    \
    } G_STMT_END

#define GDK_DEBUG_CHECK(type) GDK_DISPLAY_DEBUG_CHECK (NULL,type)
#define GDK_DEBUG(type,...) GDK_DISPLAY_DEBUG (NULL,type,__VA_ARGS__)

typedef struct
{
  const char *key;
  guint value;
  const char *help;
} GdkDebugKey;

guint gdk_parse_debug_var (const char        *variable,
                           const char        *docs,
                           const GdkDebugKey *keys,
                           guint              nkeys);

G_END_DECLS
