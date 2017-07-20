/* gtkplacessidebarprivate.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#ifndef __GTK_PLACES_SIDEBAR_PRIVATE_H__
#define __GTK_PLACES_SIDEBAR_PRIVATE_H__

#include <glib.h>
#include "gtkplacessidebar.h"

G_BEGIN_DECLS

/* Keep order, since it's used for the sort functions */
typedef enum {
  SECTION_INVALID,
  SECTION_COMPUTER,
  SECTION_MOUNTS,
  SECTION_CLOUD,
  SECTION_BOOKMARKS,
  SECTION_OTHER_LOCATIONS,
  N_SECTIONS
} GtkPlacesSidebarSectionType;

typedef enum {
  PLACES_INVALID,
  PLACES_BUILT_IN,
  PLACES_XDG_DIR,
  PLACES_MOUNTED_VOLUME,
  PLACES_BOOKMARK,
  PLACES_HEADING,
  PLACES_CONNECT_TO_SERVER,
  PLACES_ENTER_LOCATION,
  PLACES_DROP_FEEDBACK,
  PLACES_BOOKMARK_PLACEHOLDER,
  PLACES_OTHER_LOCATIONS,
  PLACES_STARRED_LOCATION,
  N_PLACES
} GtkPlacesSidebarPlaceType;

gchar *gtk_places_sidebar_get_location_title (GtkPlacesSidebar *sidebar);

G_END_DECLS

#endif /* __GTK_PLACES_SIDEBAR_PRIVATE_H__ */
