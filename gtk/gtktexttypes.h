/* gtktexttypes.h
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __GTK_TEXT_TYPES_H__
#define __GTK_TEXT_TYPES_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
# error "Only <gtk/gtk.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

#define GTK_TYPE_TEXT_UNIT G_TYPE_DOUBLE

typedef double GtkTextUnit;

typedef struct _GtkTextRectangle
{
  GtkTextUnit x;
  GtkTextUnit y;
  GtkTextUnit width;
  GtkTextUnit height;
} GtkTextRectangle;

G_END_DECLS

#endif /* __GTK_TEXT_TYPES_H__ */
