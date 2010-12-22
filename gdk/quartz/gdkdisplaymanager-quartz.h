/* gdkdisplaymanager-quartz.h
 *
 * Copyright (C) 2005-2007  Imendio AB
 * Copyright 2010 Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_QUARTZ_DISPLAYMANAGER_H__
#define __GDK_QUARTZ_DISPLAYMANAGER_H__

#include <gdk/gdk.h>
#include <gdk/gdkdisplaymanagerprivate.h>

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_DISPLAY_MANAGER    (gdk_quartz_display_manager_get_type ())
#define GDK_QUARTZ_DISPLAY_MANAGER(object) (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_QUARTZ_DISPLAY_MANAGER, GdkQuartzDisplayManager))

typedef struct _GdkQuartzDisplayManager GdkQuartzDisplayManager;
typedef struct _GdkDisplayManagerClass GdkQuartzDisplayManagerClass;

struct _GdkQuartzDisplayManager
{
  GdkDisplayManager parent;

  GdkDisplay *default_display;
  GSList *displays;
};

GType gdk_quartz_display_manager_get_type (void);

G_END_DECLS

#endif /* __GDK_QUARTZ_DISPLAYMANAGER_H__ */
