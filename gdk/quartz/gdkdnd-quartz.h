/* gdkdnd-quartz.h
 *
 * Copyright (C) 2005 Imendio AB
 * Copyright (C) 2010  Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GDK_QUARTZ_DND__
#define __GDK_QUARTZ_DND__

#include <gdkdndprivate.h>
#include "gdkquartzdnd.h"

#include <AppKit/AppKit.h>

G_BEGIN_DECLS

struct _GdkQuartzDragContext
{
  GdkDragContext context;

  id <NSDraggingInfo> dragging_info;
  GdkDevice *device;
};

struct _GdkQuartzDragContextClass
{
  GdkDragContextClass context_class;
};

G_END_DECLS

#endif /* __GDK_QUARTZ_DND__ */
