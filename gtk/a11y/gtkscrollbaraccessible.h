/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001 Sun Microsystems Inc.
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

#ifndef __GTK_SCROLLBAR_ACCESSIBLE_H__
#define __GTK_SCROLLBAR_ACCESSIBLE_H__

#include "gtkrangeaccessible.h"

G_BEGIN_DECLS

#define GTK_TYPE_SCROLLBAR_ACCESSIBLE                     (_gtk_scrollbar_accessible_get_type ())
#define GTK_SCROLLBAR_ACCESSIBLE(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SCROLLBAR_ACCESSIBLE, GtkScrollbarAccessible))
#define GTK_SCROLLBAR_ACCESSIBLE_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SCROLLBAR_ACCESSIBLE, GtkScrollbarAccessibleClass))
#define GTK_IS_SCROLLBAR_ACCESSIBLE(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SCROLLBAR_ACCESSIBLE))
#define GTK_IS_SCROLLBAR_ACCESSIBLE_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SCROLLBAR_ACCESSIBLE))
#define GTK_SCROLLBAR_ACCESSIBLE_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SCROLLBAR_ACCESSIBLE, GtkScrollbarAccessibleClass))

typedef struct _GtkScrollbarAccessible        GtkScrollbarAccessible;
typedef struct _GtkScrollbarAccessibleClass   GtkScrollbarAccessibleClass;
typedef struct _GtkScrollbarAccessiblePrivate GtkScrollbarAccessiblePrivate;

struct _GtkScrollbarAccessible
{
  GtkRangeAccessible parent;

  GtkScrollbarAccessiblePrivate *priv;
};

struct _GtkScrollbarAccessibleClass
{
  GtkRangeAccessibleClass parent_class;
};

GType _gtk_scrollbar_accessible_get_type (void);

G_END_DECLS

#endif /* __GTK_SCROLLBAR_ACCESSIBLE_H__ */
