/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Benjamin Otte
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

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif


#include <gdk/gdk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

#include <graphene.h>

G_BEGIN_DECLS

/**
 * GtkScrollInfoCenter:
 * @GTK_SCROLL_INFO_CENTER_NONE: Don't do anything
 * @GTK_SCROLL_INFO_CENTER_ROW: When scrolling vertically
 *   to a row item, this will center the row item along the
 *   visible part of list. If row item was already in the
 *   visible part, this will do nothing.
 * @GTK_SCROLL_INFO_CENTER_ROW_ALWAYS: Same as `GTK_SCROLL_INFO_CENTER_ROW`
 *   but will center the item even if it's already in the
 *   visible part of list.
 * @GTK_SCROLL_INFO_CENTER_COL: When scrolling horizontally
 *   to a column, this will center the column item across the
 *   visible part of list. If col item was already in the
 *   visible part of list, this will do nothing.
 * @GTK_SCROLL_INFO_CENTER_COL_ALWAYS: Same as `GTK_SCROLL_INFO_CENTER_COL`
 *   but will center the item even if it's already in the
 *   visible part of list.
 *
 * How we would like to center target item when scrolling to it.
 *
 * Since: 4.14
 */
typedef enum {
  GTK_SCROLL_INFO_CENTER_NONE       = 0,
  GTK_SCROLL_INFO_CENTER_ROW        = 1 << 0,
  GTK_SCROLL_INFO_CENTER_ROW_ALWAYS = 1 << 1,
  GTK_SCROLL_INFO_CENTER_COL        = 1 << 2,
  GTK_SCROLL_INFO_CENTER_COL_ALWAYS = 1 << 3
} GtkScrollInfoCenter;

#define GTK_TYPE_SCROLL_INFO    (gtk_scroll_info_get_type ())

GDK_AVAILABLE_IN_4_12
GType                   gtk_scroll_info_get_type                (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_4_12
GtkScrollInfo *         gtk_scroll_info_new                     (void);

GDK_AVAILABLE_IN_4_12
GtkScrollInfo *         gtk_scroll_info_ref                     (GtkScrollInfo           *self);
GDK_AVAILABLE_IN_4_12
void                    gtk_scroll_info_unref                   (GtkScrollInfo           *self);

GDK_AVAILABLE_IN_4_12
void                    gtk_scroll_info_set_enable_horizontal   (GtkScrollInfo           *self,
                                                                 gboolean                 horizontal);
GDK_AVAILABLE_IN_4_12
gboolean                gtk_scroll_info_get_enable_horizontal   (GtkScrollInfo           *self);

GDK_AVAILABLE_IN_4_12
void                    gtk_scroll_info_set_enable_vertical     (GtkScrollInfo           *self,
                                                                 gboolean                 vertical);
GDK_AVAILABLE_IN_4_12
gboolean                gtk_scroll_info_get_enable_vertical     (GtkScrollInfo           *self);

GDK_AVAILABLE_IN_4_14
void                    gtk_scroll_info_set_center_flags        (GtkScrollInfo           *self,
                                                                 GtkScrollInfoCenter      flags);
GDK_AVAILABLE_IN_4_14
GtkScrollInfoCenter     gtk_scroll_info_get_center_flags        (GtkScrollInfo           *self);


G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkScrollInfo, gtk_scroll_info_unref)

G_END_DECLS

