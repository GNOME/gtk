/* gtkcellrendereraccel.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#ifndef __GTK_CELL_RENDERER_ACCEL_H__
#define __GTK_CELL_RENDERER_ACCEL_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcellrenderertext.h>

G_BEGIN_DECLS

#define GTK_TYPE_CELL_RENDERER_ACCEL            (gtk_cell_renderer_accel_get_type ())
#define GTK_CELL_RENDERER_ACCEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_RENDERER_ACCEL, GtkCellRendererAccel))
#define GTK_IS_CELL_RENDERER_ACCEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_ACCEL))

typedef struct _GtkCellRendererAccel GtkCellRendererAccel;

/**
 * GtkCellRendererAccelMode:
 * @GTK_CELL_RENDERER_ACCEL_MODE_GTK: GTK+ accelerators mode
 * @GTK_CELL_RENDERER_ACCEL_MODE_OTHER: Plane accelerator mode
 * @GTK_CELL_RENDERER_ACCEL_MODE_DOUBLE_MODIFIER: Accept modifer x modifier keys. E.g. Contrl-Shift or Shift-Alt keys
 * @GTK_CELL_RENDERER_ACCEL_MODE_SINGLE_MODIFIER: Accept single modifier keys. E.g. Shift or Alt keys
 *
 * Determines if the edited accelerators are GTK+ accelerators. If
 * they are, consumed modifiers are suppressed, only accelerators
 * accepted by GTK+ are allowed, and the accelerators are rendered
 * in the same way as they are in menus. OTHER, DOUBLE_MODIFIER,
 * SINGLE_MODIFIER are mutually exclusive.
 */
typedef enum
{
  GTK_CELL_RENDERER_ACCEL_MODE_GTK,
  GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
  GTK_CELL_RENDERER_ACCEL_MODE_DOUBLE_MODIFIER,
  GTK_CELL_RENDERER_ACCEL_MODE_SINGLE_MODIFIER
} GtkCellRendererAccelMode;


GDK_AVAILABLE_IN_ALL
GType            gtk_cell_renderer_accel_get_type        (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkCellRenderer *gtk_cell_renderer_accel_new             (void);


G_END_DECLS


#endif /* __GTK_CELL_RENDERER_ACCEL_H__ */
