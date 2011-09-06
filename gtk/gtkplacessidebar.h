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
 *  Author : Mr Jamie McCracken (jamiemcc at blueyonder dot co dot uk)
 *
 */
#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_PLACES_SIDEBAR_H__
#define __GTK_PLACES_SIDEBAR_H__

#include <gtk/gtkwidget.h>

#define GTK_TYPE_PLACES_SIDEBAR			(gtk_places_sidebar_get_type ())
#define GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebar))
#define GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))
#define GTK_IS_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_IS_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))


GType gtk_places_sidebar_get_type (void);
GtkWidget *gtk_places_sidebar_new (void);

G_END_DECLS

#endif /* __GTK_PLACES_SIDEBAR_H__ */
