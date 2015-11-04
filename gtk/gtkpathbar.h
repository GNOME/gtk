/* gtkpathbar.h
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Soriano <csoriano@gnome.org>
 */

#ifndef __GTK_PATH_BAR_H__
#define __GTK_PATH_BAR_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkstack.h>

G_BEGIN_DECLS

#define GTK_TYPE_PATH_BAR            (gtk_path_bar_get_type())
#define GTK_PATH_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR, GtkPathBar))
#define GTK_PATH_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_PATH_BAR, GtkPathBarClass))
#define GTK_IS_PATH_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR))
#define GTK_IS_PATH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_PATH_BAR)
#define GTK_PATH_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_PATH_BAR, GtkPathBarClass))

typedef struct _GtkPathBar GtkPathBar;
typedef struct _GtkPathBarClass GtkPathBarClass;
typedef struct _GtkPathBarPrivate GtkPathBarPrivate;

struct _GtkPathBarClass
{
  GtkStackClass parent;

  /*< public >*/

  void                (* populate_popup)              (GtkPathBar       *path_bar,
                                                       const gchar      *selected_path);
  /*< private >*/

  /* Padding for future expansion */
  gpointer reserved[10];
};

struct _GtkPathBar
{
  GtkStack parent_instance;
};

GDK_AVAILABLE_IN_3_20
GType                 gtk_path_bar_get_type              (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_3_20
GtkWidget*            gtk_path_bar_new                   (void);
GDK_AVAILABLE_IN_3_20
void                  gtk_path_bar_set_path              (GtkPathBar       *path_bar,
                                                          const gchar      *path);
GDK_AVAILABLE_IN_3_20
const gchar*          gtk_path_bar_get_path              (GtkPathBar       *path_bar);

GDK_AVAILABLE_IN_3_20
void                  gtk_path_bar_set_path_extended     (GtkPathBar       *self,
                                                          const gchar      *path,
                                                          const gchar      *root_path,
                                                          const gchar      *root_label,
                                                          GIcon            *root_icon);

GDK_AVAILABLE_IN_3_20
void                  gtk_path_bar_set_selected_path     (GtkPathBar       *path_bar,
                                                          const gchar      *path);
GDK_AVAILABLE_IN_3_20
const gchar*          gtk_path_bar_get_selected_path     (GtkPathBar       *path_bar);

GDK_AVAILABLE_IN_3_20
void                  gtk_path_bar_set_inverted          (GtkPathBar       *path_bar,
                                                          gboolean          inverted);
GDK_AVAILABLE_IN_3_20
gboolean              gtk_path_bar_get_inverted          (GtkPathBar       *path_bar);

G_END_DECLS

#endif /* __GTK_PATH_BAR_H__ */

