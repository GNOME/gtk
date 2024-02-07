/*
 * gdkdmabuf-wayland.h
 *
 * Copyright 2023 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "config.h"

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-egl.h>

#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdksurface.h>

G_BEGIN_DECLS

typedef struct
{
  uint32_t fourcc;
  uint32_t padding;
  uint64_t modifier;
} DmabufFormat;

typedef struct
{
  dev_t target_device;
  guint32 flags;
  gsize n_formats;
  DmabufFormat *formats;
} DmabufTranche;

typedef struct
{
  dev_t main_device;
  GPtrArray *tranches;
} DmabufFormats;

typedef struct DmabufFormatsInfo DmabufFormatsInfo;

typedef void (* DmabufFormatsUpdateCallback) (gpointer           data,
                                              DmabufFormatsInfo *formats);

struct DmabufFormatsInfo
{
  char *name;
  struct zwp_linux_dmabuf_feedback_v1 *feedback;

  DmabufFormatsUpdateCallback callback;
  gpointer data;

  gsize n_dmabuf_formats;
  DmabufFormat *dmabuf_format_table;

  DmabufFormats *dmabuf_formats;
  DmabufFormats *pending_dmabuf_formats;
  DmabufTranche *pending_tranche;

  GdkDmabufFormats *egl_formats;
  GdkDmabufFormats *formats;
};

DmabufFormatsInfo * dmabuf_formats_info_new  (const char                          *name,
                                              GdkDmabufFormats                    *egl_formats,
                                              struct zwp_linux_dmabuf_feedback_v1 *feedback,
                                              DmabufFormatsUpdateCallback          callback,
                                              gpointer                             data);

void                dmabuf_formats_info_free (DmabufFormatsInfo                   *info);

void                dmabuf_formats_info_set_egl_formats
                                             (DmabufFormatsInfo                   *info,
                                              GdkDmabufFormats                    *egl_formats);

G_END_DECLS
