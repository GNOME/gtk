/* gtkplacesview.h
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef GTK_PLACES_VIEW_H
#define GTK_PLACES_VIEW_H

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkbox.h>
#include <gtk/gtkplacessidebar.h>

G_BEGIN_DECLS

#define GTK_TYPE_PLACES_VIEW        (gtk_places_view_get_type ())
#define GTK_PLACES_VIEW(obj)        (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PLACES_VIEW, GtkPlacesView))
#define GTK_PLACES_VIEW_CLASS(klass)(G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PLACES_VIEW, GtkPlacesViewClass))
#define GTK_IS_PLACES_VIEW(obj)     (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PLACES_VIEW))
#define GTK_IS_PLACES_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PLACES_VIEW))
#define GTK_PLACES_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PLACES_VIEW, GtkPlacesViewClass))

typedef struct _GtkPlacesView GtkPlacesView;
typedef struct _GtkPlacesViewClass GtkPlacesViewClass;
typedef struct _GtkPlacesViewPrivate GtkPlacesViewPrivate;

struct _GtkPlacesViewClass
{
  GtkBoxClass parent_class;

  void     (* open_location)        (GtkPlacesView          *view,
                                     GFile                  *location,
                                     GtkPlacesOpenFlags  open_flags);

  void    (* show_error_message)     (GtkPlacesSidebar      *sidebar,
                                      const gchar           *primary,
                                      const gchar           *secondary);

  /*< private >*/

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkPlacesView
{
  GtkBox parent_instance;
};

GDK_AVAILABLE_IN_3_18
GType              gtk_places_view_get_type                      (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_18
GtkPlacesOpenFlags gtk_places_view_get_open_flags                (GtkPlacesView      *view);
GDK_AVAILABLE_IN_3_18
void               gtk_places_view_set_open_flags                (GtkPlacesView      *view,
                                                                  GtkPlacesOpenFlags  flags);

GDK_AVAILABLE_IN_3_18
const gchar*       gtk_places_view_get_search_query              (GtkPlacesView      *view);
GDK_AVAILABLE_IN_3_18
void               gtk_places_view_set_search_query              (GtkPlacesView      *view,
                                                                  const gchar        *query_text);

GDK_AVAILABLE_IN_3_18
gboolean           gtk_places_view_get_local_only                (GtkPlacesView         *view);

GDK_AVAILABLE_IN_3_18
void               gtk_places_view_set_local_only                (GtkPlacesView         *view,
                                                                  gboolean               local_only);

GDK_AVAILABLE_IN_3_18
gboolean           gtk_places_view_get_loading                   (GtkPlacesView         *view);

GDK_AVAILABLE_IN_3_18
GtkWidget *        gtk_places_view_new                           (void);

G_END_DECLS

#endif /* GTK_PLACES_VIEW_H */
