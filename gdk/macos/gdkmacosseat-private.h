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

#ifndef __GDK_MACOS_SEAT_PRIVATE_H__
#define __GDK_MACOS_SEAT_PRIVATE_H__

#include <AppKit/AppKit.h>

#include "gdkmacosdisplay.h"

#include "gdkseatprivate.h"

G_BEGIN_DECLS

GdkSeat *_gdk_macos_seat_new (GdkMacosDisplay *display);

G_END_DECLS

#endif /* __GDK_MACOS_SEAT_PRIVATE_H__ */
