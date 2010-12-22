/* gdkcursor-quartz.h
 *
 * Copyright (C) 2005-2007  Imendio AB
 * Copyright (C) 2010 Kristian Rietveld  <kris@gtk.org>
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

#ifndef __GDK_QUARTZ_CURSOR_H__
#define __GDK_QUARTZ_CURSOR_H__

#include <gdk/gdk.h>
#include <gdk/quartz/gdkquartz.h>
#include "gdkcursorprivate.h"

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_CURSOR              (gdk_quartz_cursor_get_type ())
#define GDK_QUARTZ_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_QUARTZ_CURSOR, GdkQuartzCursor))
#define GDK_QUARTZ_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_QUARTZ_CURSOR, GdkQuartzCursorClass))
#define GDK_IS_QUARTZ_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_QUARTZ_CURSOR))
#define GDK_IS_QUARTZ_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_QUARTZ_CURSOR))
#define GDK_QUARTZ_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_QUARTZ_CURSOR, GdkQuartzCursorClass))

typedef struct _GdkQuartzCursor GdkQuartzCursor;
typedef struct _GdkQuartzCursorClass GdkQuartzCursorClass;

struct _GdkQuartzCursor
{
  GdkCursor cursor;

  NSCursor *nscursor;
};

struct _GdkQuartzCursorClass
{
  GdkCursorClass cursor_class;
};

GType gdk_quartz_cursor_get_type (void);

G_END_DECLS

#endif /* __GDK_QUARTZ_DRAG_CONTEXT_H__ */
