/*
 * Copyright © 2020 Benjamin Otte
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

#ifndef __OTTIE_PAINTABLE_H__
#define __OTTIE_PAINTABLE_H__

#if !defined (__OTTIE_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <ottie/ottie.h> can be included directly."
#endif

#include <ottie/ottiecreation.h>

G_BEGIN_DECLS

#define OTTIE_TYPE_PAINTABLE (ottie_paintable_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (OttiePaintable, ottie_paintable, OTTIE, PAINTABLE, GObject)

GDK_AVAILABLE_IN_ALL
OttiePaintable *        ottie_paintable_new                (OttieCreation       *creation);

GDK_AVAILABLE_IN_ALL
OttieCreation *         ottie_paintable_get_creation       (OttiePaintable      *self);
GDK_AVAILABLE_IN_ALL
void                    ottie_paintable_set_creation       (OttiePaintable      *self,
                                                            OttieCreation       *creation);
GDK_AVAILABLE_IN_ALL
gint64                  ottie_paintable_get_timestamp      (OttiePaintable      *self);
GDK_AVAILABLE_IN_ALL
void                    ottie_paintable_set_timestamp      (OttiePaintable      *self,
                                                            gint64               timestamp);
GDK_AVAILABLE_IN_ALL
gint64                  ottie_paintable_get_duration       (OttiePaintable      *self);

G_END_DECLS

#endif /* __OTTIE_PAINTABLE_H__ */
