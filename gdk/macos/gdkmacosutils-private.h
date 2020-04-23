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

#ifndef __GDK_MACOS_UTILS_PRIVATE_H__
#define __GDK_MACOS_UTILS_PRIVATE_H__

#include <AppKit/AppKit.h>
#include <gdk/gdk.h>

#define GDK_BEGIN_MACOS_ALLOC_POOL NSAutoreleasePool *_pool = [[NSAutoreleasePool alloc] init]
#define GDK_END_MACOS_ALLOC_POOL   [_pool release]

#endif /* __GDK_MACOS_UTILS_PRIVATE_H__ */
