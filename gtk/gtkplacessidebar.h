/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  GtkPlacesSidebar - sidebar widget for places in the filesystem
 *
 *  This code comes from Nautilus, GNOME's file manager.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *            Federico Mena Quintero <federico@gnome.org>
 *
 */
#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_PLACES_SIDEBAR_H__
#define __GTK_PLACES_SIDEBAR_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_PLACES_SIDEBAR			(gtk_places_sidebar_get_type ())
#define GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebar))
#define GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))
#define GTK_IS_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_IS_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))

typedef struct _GtkPlacesSidebar GtkPlacesSidebar;
typedef struct _GtkPlacesSidebarClass GtkPlacesSidebarClass;

typedef enum {
  GTK_PLACES_OPEN_MODE_NORMAL,
  GTK_PLACES_OPEN_MODE_NEW_TAB,
  GTK_PLACES_OPEN_MODE_NEW_WINDOW
} GtkPlacesOpenMode;

GType gtk_places_sidebar_get_type (void);
GtkWidget *gtk_places_sidebar_new (void);

/* FIXME: add GObject properties for the following things */

void gtk_places_sidebar_set_current_location (GtkPlacesSidebar *sidebar, GFile *location);

void gtk_places_sidebar_set_multiple_tabs_supported (GtkPlacesSidebar *sidebar, gboolean supported);

void gtk_places_sidebar_set_multiple_windows_supported (GtkPlacesSidebar *sidebar, gboolean supported);

void gtk_places_sidebar_set_show_desktop (GtkPlacesSidebar *sidebar, gboolean show_desktop);

void gtk_places_sidebar_set_show_properties (GtkPlacesSidebar *sidebar, gboolean show_properties);

void gtk_places_sidebar_set_show_trash (GtkPlacesSidebar *sidebar, gboolean show_trash);
void gtk_places_sidebar_set_trash_is_full (GtkPlacesSidebar *sidebar, gboolean is_full);

void gtk_places_sidebar_set_show_cwd (GtkPlacesSidebar *sidebar, gboolean show_cwd);

void gtk_places_sidebar_set_file_dnd_enabled (GtkPlacesSidebar *sidebar, gboolean file_dnd_enabled);

G_END_DECLS

#endif /* __GTK_PLACES_SIDEBAR_H__ */
