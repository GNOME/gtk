/*
 * Copyright © 2018 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GDK_DROP_PRIVATE_H__
#define __GDK_DROP_PRIVATE_H__

#include "gdkdrop.h"

G_BEGIN_DECLS


#define GDK_DROP_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DROP, GdkDropClass))
#define GDK_IS_DROP_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DROP))
#define GDK_DROP_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DROP, GdkDropClass))

typedef struct _GdkDropClass GdkDropClass;


struct _GdkDrop {
  GObject parent_instance;
};

struct _GdkDropClass {
  GObjectClass parent_class;
};


G_END_DECLS

#endif
