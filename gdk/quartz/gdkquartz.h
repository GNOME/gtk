
/* gdkquartz.h
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#ifndef __GDK_QUARTZ_H__
#define __GDK_QUARTZ_H__

#include <AppKit/AppKit.h>

#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>

G_BEGIN_DECLS

/* NSInteger only exists in Leopard and newer.  This check has to be
 * done after inclusion of the system headers.  If NSInteger has not
 * been defined, we know for sure that we are on 32-bit.
 */
#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#endif

#ifndef CGFLOAT_DEFINED
typedef float CGFloat;
#endif

typedef enum
{
  GDK_OSX_UNSUPPORTED = 0,
  GDK_OSX_MIN = 4,
  GDK_OSX_TIGER = 4,
  GDK_OSX_LEOPARD = 5,
  GDK_OSX_SNOW_LEOPARD = 6,
  GDK_OSX_LION = 7,
  GDK_OSX_MOUNTAIN_LION = 8,
  GDK_OSX_CURRENT = 8,
  GDK_OSX_NEW = 99
} GdkOSXVersion;

GdkOSXVersion gdk_quartz_osx_version (void);

GdkAtom   gdk_quartz_pasteboard_type_to_atom_libgtk_only        (NSString       *type);
NSString *gdk_quartz_target_to_pasteboard_type_libgtk_only      (const gchar    *target);
NSString *gdk_quartz_atom_to_pasteboard_type_libgtk_only        (GdkAtom         atom);

G_END_DECLS

#define __GDKQUARTZ_H_INSIDE__

#include <gdk/quartz/gdkquartzcursor.h>
#include <gdk/quartz/gdkquartzdevice-core.h>
#include <gdk/quartz/gdkquartzdevicemanager-core.h>
#include <gdk/quartz/gdkquartzdisplay.h>
#include <gdk/quartz/gdkquartzdisplaymanager.h>
#include <gdk/quartz/gdkquartzdnd.h>
#include <gdk/quartz/gdkquartzkeys.h>
#include <gdk/quartz/gdkquartzscreen.h>
#include <gdk/quartz/gdkquartzutils.h>
#include <gdk/quartz/gdkquartzvisual.h>
#include <gdk/quartz/gdkquartzwindow.h>

#undef __GDKQUARTZ_H_INSIDE__

#endif /* __GDK_QUARTZ_H__ */
