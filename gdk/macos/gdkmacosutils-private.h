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

#pragma once

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#define GDK_BEGIN_MACOS_ALLOC_POOL NSAutoreleasePool *_pool = [[NSAutoreleasePool alloc] init]
#define GDK_END_MACOS_ALLOC_POOL   [_pool release]

static inline gboolean
queue_contains (GQueue *queue,
                GList  *link_)
{
  return queue->head == link_ || link_->prev || link_->next;
}

struct _GdkPoint
{
  int x;
  int y;
};
typedef struct _GdkPoint GdkPoint;

static inline NSPoint
convert_nspoint_from_screen (NSWindow *window,
                             NSPoint   point)
{
  return [window convertPointFromScreen:point];
}

static inline NSPoint
convert_nspoint_to_screen (NSWindow *window,
                           NSPoint   point)
{
  return [window convertPointToScreen:point];
}

