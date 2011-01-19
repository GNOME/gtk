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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
