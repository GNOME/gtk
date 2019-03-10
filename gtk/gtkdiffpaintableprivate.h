/*
 * Copyright © 2019 Benjamin Otte
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

#ifndef __GTK_DIFF_PAINTABLE_H__
#define __GTK_DIFF_PAINTABLE_H__

#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GTK_TYPE_DIFF_PAINTABLE (gtk_diff_paintable_get_type ())

G_DECLARE_FINAL_TYPE (GtkDiffPaintable, gtk_diff_paintable, GTK, DIFF_PAINTABLE, GObject)

GdkPaintable *  gtk_diff_paintable_new                  (GdkPaintable           *first,
                                                         GdkPaintable           *second);

void            gtk_diff_paintable_set_first            (GtkDiffPaintable       *self,
                                                         GdkPaintable           *first);
GdkPaintable *  gtk_diff_paintable_get_first            (GtkDiffPaintable       *self) G_GNUC_PURE;
void            gtk_diff_paintable_set_second           (GtkDiffPaintable       *self,
                                                         GdkPaintable           *second);
GdkPaintable *  gtk_diff_paintable_get_second           (GtkDiffPaintable       *self) G_GNUC_PURE;

G_END_DECLS

#endif /* __GTK_DIFF_PAINTABLE_H__ */
