/* gtkscrollable.h
 * Copyright (C) 2008 Tadej Borovšak <tadeboro@gmail.com>
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

#ifndef __GTK_SCROLLABLE_H__
#define __GTK_SCROLLABLE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_SCROLLABLE            (gtk_scrollable_get_type ())
#define GTK_SCROLLABLE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),     GTK_TYPE_SCROLLABLE, GtkScrollable))
#define GTK_IS_SCROLLABLE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),     GTK_TYPE_SCROLLABLE))
#define GTK_SCROLLABLE_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_SCROLLABLE, GtkScrollableInterface))

typedef struct _GtkScrollable          GtkScrollable; /* Dummy */
typedef struct _GtkScrollableInterface GtkScrollableInterface;

struct _GtkScrollableInterface
{
  GTypeInterface base_iface;
};

/* Public API */
GType                gtk_scrollable_get_type               (void) G_GNUC_CONST;
GtkAdjustment       *gtk_scrollable_get_hadjustment        (GtkScrollable       *scrollable);
void                 gtk_scrollable_set_hadjustment        (GtkScrollable       *scrollable,
							    GtkAdjustment       *hadjustment);
GtkAdjustment       *gtk_scrollable_get_vadjustment        (GtkScrollable       *scrollable);
void                 gtk_scrollable_set_vadjustment        (GtkScrollable       *scrollable,
							    GtkAdjustment       *vadjustment);
GtkScrollablePolicy  gtk_scrollable_get_hscroll_policy     (GtkScrollable       *scrollable);
void                 gtk_scrollable_set_hscroll_policy     (GtkScrollable       *scrollable,
							    GtkScrollablePolicy  policy);
GtkScrollablePolicy  gtk_scrollable_get_vscroll_policy     (GtkScrollable       *scrollable);
void                 gtk_scrollable_set_vscroll_policy     (GtkScrollable       *scrollable,
							    GtkScrollablePolicy  policy);

G_END_DECLS

#endif /* __GTK_SCROLLABLE_H__ */
