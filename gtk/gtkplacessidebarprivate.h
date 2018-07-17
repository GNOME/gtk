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
#include <gtk/gtkwidget.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_PLACES_SIDEBAR			(gtk_places_sidebar_get_type ())
#define GTK_PLACES_SIDEBAR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebar))
#define GTK_PLACES_SIDEBAR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))
#define GTK_IS_PLACES_SIDEBAR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_IS_PLACES_SIDEBAR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLACES_SIDEBAR))
#define GTK_PLACES_SIDEBAR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PLACES_SIDEBAR, GtkPlacesSidebarClass))

typedef struct _GtkPlacesSidebar GtkPlacesSidebar;
typedef struct _GtkPlacesSidebarClass GtkPlacesSidebarClass;

/*
 * GtkPlacesOpenFlags:
 * @GTK_PLACES_OPEN_NORMAL: This is the default mode that #GtkPlacesSidebar uses if no other flags
 *  are specified.  It indicates that the calling application should open the selected location
 *  in the normal way, for example, in the folder view beside the sidebar.
 * @GTK_PLACES_OPEN_NEW_TAB: When passed to gtk_places_sidebar_set_open_flags(), this indicates
 *  that the application can open folders selected from the sidebar in new tabs.  This value
 *  will be passed to the #GtkPlacesSidebar::open-location signal when the user selects
 *  that a location be opened in a new tab instead of in the standard fashion.
 * @GTK_PLACES_OPEN_NEW_WINDOW: Similar to @GTK_PLACES_OPEN_NEW_TAB, but indicates that the application
 *  can open folders in new windows.
 *
 * These flags serve two purposes.  First, the application can call gtk_places_sidebar_set_open_flags()
 * using these flags as a bitmask.  This tells the sidebar that the application is able to open
 * folders selected from the sidebar in various ways, for example, in new tabs or in new windows in
 * addition to the normal mode.
 *
 * Second, when one of these values gets passed back to the application in the
 * #GtkPlacesSidebar::open-location signal, it means that the application should
 * open the selected location in the normal way, in a new tab, or in a new
 * window.  The sidebar takes care of determining the desired way to open the location,
 * based on the modifier keys that the user is pressing at the time the selection is made.
 *
 * If the application never calls gtk_places_sidebar_set_open_flags(), then the sidebar will only
 * use #GTK_PLACES_OPEN_NORMAL in the #GtkPlacesSidebar::open-location signal.  This is the
 * default mode of operation.
 */

GType              gtk_places_sidebar_get_type                   (void) G_GNUC_CONST;
GtkWidget *        gtk_places_sidebar_new                        (void);

GtkPlacesOpenFlags gtk_places_sidebar_get_open_flags             (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_open_flags             (GtkPlacesSidebar   *sidebar,
                                                                  GtkPlacesOpenFlags  flags);

GFile *            gtk_places_sidebar_get_location               (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_location               (GtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);

gboolean           gtk_places_sidebar_get_show_recent            (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_show_recent            (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_recent);

gboolean           gtk_places_sidebar_get_show_desktop           (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_show_desktop           (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_desktop);

gboolean           gtk_places_sidebar_get_show_enter_location    (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_show_enter_location    (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_enter_location);

void                 gtk_places_sidebar_set_local_only           (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            local_only);
gboolean             gtk_places_sidebar_get_local_only           (GtkPlacesSidebar   *sidebar);


void               gtk_places_sidebar_add_shortcut               (GtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);
void               gtk_places_sidebar_remove_shortcut            (GtkPlacesSidebar   *sidebar,
                                                                  GFile              *location);
GSList *           gtk_places_sidebar_list_shortcuts             (GtkPlacesSidebar   *sidebar);

GFile *            gtk_places_sidebar_get_nth_bookmark           (GtkPlacesSidebar   *sidebar,
                                                                  gint                n);
void               gtk_places_sidebar_set_drop_targets_visible   (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            visible,
                                                                  GdkDrag            *drag);
gboolean           gtk_places_sidebar_get_show_trash             (GtkPlacesSidebar   *sidebar);
void               gtk_places_sidebar_set_show_trash             (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_trash);

void                 gtk_places_sidebar_set_show_other_locations (GtkPlacesSidebar   *sidebar,
                                                                  gboolean            show_other_locations);
gboolean             gtk_places_sidebar_get_show_other_locations (GtkPlacesSidebar   *sidebar);

void                 gtk_places_sidebar_set_show_starred_location (GtkPlacesSidebar   *sidebar,
                                                                   gboolean            show_starred_location);
gboolean             gtk_places_sidebar_get_show_starred_location (GtkPlacesSidebar   *sidebar);

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
