/* gtkshortcutsviewprivate.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_SHORTCUTS_VIEW_H__
#define __GTK_SHORTCUTS_VIEW_H__

#include <gtk/gtkbox.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHORTCUTS_VIEW (gtk_shortcuts_view_get_type ())
#define GTK_SHORTCUTS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SHORTCUTS_VIEW, GtkShortcutsView))
#define GTK_SHORTCUTS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SHORTCUTS_VIEW, GtkShortcutsViewClass))
#define GTK_IS_SHORTCUTS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SHORTCUTS_VIEW))
#define GTK_IS_SHORTCUTS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SHORTCUTS_VIEW))
#define GTK_SHORTCUTS_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SHORTCUTS_VIEW, GtkShortcutsViewClass))


typedef struct _GtkShortcutsView      GtkShortcutsView;
typedef struct _GtkShortcutsViewClass GtkShortcutsViewClass;


GType        gtk_shortcuts_view_get_type (void) G_GNUC_CONST;

const gchar *gtk_shortcuts_view_get_view_name (GtkShortcutsView *self);
const gchar *gtk_shortcuts_view_get_title     (GtkShortcutsView *self);

G_END_DECLS

#endif /* __GTK_SHORTCUTS_VIEW_H__ */
