/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkinternals.h"

#include "gdkmir.h"

#define GDK_MIR_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WINDOW_MIR, GdkMirWindow))
#define GDK_MIR_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WINDOW_MIR, GdkMirWindowClass))
#define GDK_IS_WINDOW_MIR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WINDOW_MIR))
#define GDK_MIR_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WINDOW_MIR, GdkMirWindowClass))

typedef struct _GdkMirWindow GdkMirWindow;
typedef struct _GdkMirWindowClass GdkMirWindowClass;

struct _GdkMirWindow
{
  GdkWindow parent_instance;
};

struct _GdkMirWindowClass
{
  GdkWindowClass parent_class;
};

G_DEFINE_TYPE (GdkMirWindow, gdk_mir_window, GDK_TYPE_WINDOW)

static void
gdk_mir_window_init (GdkMirWindow *impl)
{
}

static void
gdk_mir_window_class_init (GdkMirWindowClass *klass)
{
}
