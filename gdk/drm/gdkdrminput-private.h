/*
 * Copyright © 2024 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gdkdrmdisplay-private.h"

G_BEGIN_DECLS

typedef struct _GdkDrmInputSource GdkDrmInputSource;

struct _GdkDrmInputSource
{
  GSource parent_instance;
  GdkDrmDisplay *display;
  GPollFD poll_fd;
};

GSource *_gdk_drm_input_source_new (GdkDrmDisplay *display);

void     _gdk_drm_input_handle_event (GdkDrmDisplay *display,
                                      void           *event);

G_END_DECLS
