/* GDK - The GIMP Drawing Kit
 *
 * Copyright © 2018  Benjamin Otte
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

#include "config.h"

#include "gdkconfig.h"

#include "gdkcairocontext-broadway.h"

G_DEFINE_TYPE (GdkBroadwayCairoContext, gdk_broadway_cairo_context, GDK_TYPE_CAIRO_CONTEXT)

static void
gdk_broadway_cairo_context_class_init (GdkBroadwayCairoContextClass *klass)
{
}

static void
gdk_broadway_cairo_context_init (GdkBroadwayCairoContext *self)
{
}

