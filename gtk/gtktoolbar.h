/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * GtkToolbar copyright (C) Federico Mena
 *
 * Copyright (C) 2002 Anders Carlsson <andersca@gnome.org>
 * Copyright (C) 2002 James Henstridge <james@daa.com.au>
 * Copyright (C) 2003 Soeren Sandmann <sandmann@daimi.au.dk>
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_TOOLBAR_H__
#define __GTK_TOOLBAR_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkcontainer.h>
#include <gtk/gtktoolitem.h>

G_BEGIN_DECLS


#define GTK_TYPE_TOOLBAR            (gtk_toolbar_get_type ())
#define GTK_TOOLBAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_TOOLBAR, GtkToolbar))
#define GTK_IS_TOOLBAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_TOOLBAR))

typedef struct _GtkToolbar GtkToolbar;

GDK_AVAILABLE_IN_ALL
GType           gtk_toolbar_get_type                (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GtkWidget *     gtk_toolbar_new                     (void);

GDK_AVAILABLE_IN_ALL
void            gtk_toolbar_insert                  (GtkToolbar      *toolbar,
						     GtkToolItem     *item,
						     gint             pos);

GDK_AVAILABLE_IN_ALL
gint            gtk_toolbar_get_item_index          (GtkToolbar      *toolbar,
						     GtkToolItem     *item);
GDK_AVAILABLE_IN_ALL
gint            gtk_toolbar_get_n_items             (GtkToolbar      *toolbar);
GDK_AVAILABLE_IN_ALL
GtkToolItem *   gtk_toolbar_get_nth_item            (GtkToolbar      *toolbar,
						     gint             n);

GDK_AVAILABLE_IN_ALL
gboolean        gtk_toolbar_get_show_arrow          (GtkToolbar      *toolbar);
GDK_AVAILABLE_IN_ALL
void            gtk_toolbar_set_show_arrow          (GtkToolbar      *toolbar,
						     gboolean         show_arrow);

GDK_AVAILABLE_IN_ALL
GtkToolbarStyle gtk_toolbar_get_style               (GtkToolbar      *toolbar);
GDK_AVAILABLE_IN_ALL
void            gtk_toolbar_set_style               (GtkToolbar      *toolbar,
						     GtkToolbarStyle  style);
GDK_AVAILABLE_IN_ALL
void            gtk_toolbar_unset_style             (GtkToolbar      *toolbar);

GDK_AVAILABLE_IN_ALL
gint            gtk_toolbar_get_drop_index          (GtkToolbar      *toolbar,
						     gint             x,
						     gint             y);
GDK_AVAILABLE_IN_ALL
void            gtk_toolbar_set_drop_highlight_item (GtkToolbar      *toolbar,
						     GtkToolItem     *tool_item,
						     gint             index_);


G_END_DECLS

#endif /* __GTK_TOOLBAR_H__ */
