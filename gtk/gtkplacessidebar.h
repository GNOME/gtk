/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *  Nautilus
 *
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
#ifndef _NAUTILUS_PLACES_SIDEBAR_H
#define _NAUTILUS_PLACES_SIDEBAR_H

#include "nautilus-window.h"

#include <gtk/gtk.h>

#define NAUTILUS_PLACES_SIDEBAR_ID    "places"

#define NAUTILUS_TYPE_PLACES_SIDEBAR nautilus_places_sidebar_get_type()
#define NAUTILUS_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebar))
#define NAUTILUS_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebarClass))
#define NAUTILUS_IS_PLACES_SIDEBAR(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_IS_PLACES_SIDEBAR_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PLACES_SIDEBAR))
#define NAUTILUS_PLACES_SIDEBAR_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PLACES_SIDEBAR, NautilusPlacesSidebarClass))


GType nautilus_places_sidebar_get_type (void);
GtkWidget * nautilus_places_sidebar_new (NautilusWindow *window);


#endif
