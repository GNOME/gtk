/* gtksimpletab.h
 *
 * Copyright (C) 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SIMPLE_TAB_H__
#define __GTK_SIMPLE_TAB_H__

#include <gtk/gtktab.h>

G_BEGIN_DECLS

#define GTK_TYPE_SIMPLE_TAB                 (gtk_simple_tab_get_type ())
#define GTK_SIMPLE_TAB(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SIMPLE_TAB, GtkSimpleTab))
#define GTK_IS_SIMPLE_TAB(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SIMPLE_TAB))

typedef struct _GtkSimpleTab      GtkSimpleTab;

GDK_AVAILABLE_IN_3_22
GType            gtk_simple_tab_get_type   (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GTK_SIMPLE_TAB_H__ */
