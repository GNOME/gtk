/* GTK - The GIMP Toolkit
 * gtkfilechooser.h: Abstract interface for file selector GUIs
 * Copyright (C) 2003, Red Hat, Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_FILE_CHOOSER_H__
#define __GTK_FILE_CHOOSER_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_FILE_CHOOSER             (gtk_file_chooser_get_type ())
#define GTK_FILE_CHOOSER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_CHOOSER, GtkFileChooser))
#define GTK_IS_FILE_CHOOSER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_CHOOSER))
#define GTK_FILE_CHOOSER_GET_IFACE(inst)  (G_TYPE_INSTANCE_GET_INTERFACE ((inst), GTK_TYPE_FILE_CHOOSER, GtkFileChooserIface))

typedef struct _GtkFileChooser      GtkFileChooser;
typedef struct _GtkFileChooserIface GtkFileChooserIface;

typedef enum
{
  GTK_FILE_CHOOSER_ACTION_OPEN,
  GTK_FILE_CHOOSER_ACTION_SAVE
} GtkFileChooserAction;

struct _GtkFileChooserIface
{
  GTypeInterface base_iface;

  /* GtkFileChooser interface has the following properties:
   *
   *  action:          GtkFileChooserAction
   *  folder_mode:     boolean
   *  select_multiple: boolean
   *  show_hidden:     boolean
   *  local_only:      boolean
   *
   *  preview_widget:  GtkWidget
   *  preview_widget_active: boolean
   */

  /* Methods
   */
  void (*set_current_folder)   (GtkFileChooser *chooser,
			        const char     *uri);
  char * (*get_current_folder) (GtkFileChooser *chooser);
  void (*select_uri)           (GtkFileChooser *chooser,
				const char     *uri);
  void (*unselect_uri)         (GtkFileChooser *chooser,
				const char     *uri);
  void (*select_all)           (GtkFileChooser *chooser);
  void (*unselect_all)         (GtkFileChooser *chooser);
  GSList *(*get_uris)          (GtkFileChooser *chooser);


  /* Signals
   */
  void (*current_folder_changed) (GtkFileChooser *chooser);
  void (*selection_changed)      (GtkFileChooser *chooser);
  void (*update_preview)         (GtkFileChooser *chooser,
				  const char     *uri);
};

GType gtk_file_chooser_get_type (void);

/* Configuration
 */
void                 gtk_file_chooser_set_action          (GtkFileChooser       *chooser,
							   GtkFileChooserAction  action);
GtkFileChooserAction gtk_file_chooser_get_action          (GtkFileChooser       *chooser);
void                 gtk_file_chooser_set_folder_mode     (GtkFileChooser       *chooser,
							   gboolean              folder_mode);
gboolean             gtk_file_chooser_get_folder_mode     (GtkFileChooser       *chooser);
void                 gtk_file_chooser_set_local_only      (GtkFileChooser       *chooser,
							   gboolean              files_only);
gboolean             gtk_file_chooser_get_local_only      (GtkFileChooser       *chooser);
void                 gtk_file_chooser_set_select_multiple (GtkFileChooser       *chooser,
							   gboolean              select_multiple);
gboolean             gtk_file_chooser_get_select_multiple (GtkFileChooser       *chooser);

/* Filename manipulation
 */
gchar * gtk_file_chooser_get_filename      (GtkFileChooser *chooser);
void    gtk_file_chooser_set_filename      (GtkFileChooser *chooser,
					    const char     *filename);
void    gtk_file_chooser_select_filename   (GtkFileChooser *chooser,
					    const char     *filename);
void    gtk_file_chooser_unselect_filename (GtkFileChooser *chooser,
					    const char     *filename);
void    gtk_file_chooser_select_all        (GtkFileChooser *chooser);
void    gtk_file_chooser_unselect_all      (GtkFileChooser *chooser);
GSList *gtk_file_chooser_get_filenames     (GtkFileChooser *chooser);
void   gtk_file_chooser_set_current_folder (GtkFileChooser *chooser,
					    const gchar    *filename);
gchar *gtk_file_chooser_get_current_folder (GtkFileChooser *chooser);


/* URI manipulation
 */
gchar * gtk_file_chooser_get_uri      (GtkFileChooser *chooser);
void    gtk_file_chooser_set_uri      (GtkFileChooser *chooser,
				       const char     *uri);
void    gtk_file_chooser_select_uri   (GtkFileChooser *chooser,
				       const char     *uri);
void    gtk_file_chooser_unselect_uri (GtkFileChooser *chooser,
				       const char     *uri);
GSList *gtk_file_chooser_get_uris     (GtkFileChooser *chooser);

void   gtk_file_chooser_set_current_folder_uri (GtkFileChooser *chooser,
						const gchar    *uri);
gchar *gtk_file_chooser_get_current_folder_uri (GtkFileChooser *chooser);

/* Preview widget
 */
void       gtk_file_chooser_set_preview_widget        (GtkFileChooser *chooser,
						       GtkWidget      *preview_widget);
GtkWidget *gtk_file_chooser_get_preview_widget        (GtkFileChooser *chooser);
void       gtk_file_chooser_set_preview_widget_active (GtkFileChooser *chooser,
						       gboolean        active);
gboolean   gtk_file_chooser_get_preview_widget_active (GtkFileChooser *chooser);

const char *gtk_file_chooser_get_preview_filename (GtkFileChooser *file_chooser);
const char *gtk_file_chooser_get_preview_uri      (GtkFileChooser *file_chooser);


#if 0
/* Filters
 */
void   gtk_file_chooser_add_filter    (GtkFileChooser *chooser,
				       GtkFileFilter  *filter);
void   gtk_file_chooser_remove_filter (GtkFileChooser *chooser,
				       GtkFileFilter  *filter);
GList *gtk_file_chooser_get_filters   (GtkFileChooser  *chooser);


/************************************************/

static gboolean (*GtkFileFilterFunc) (const char *uri,
				      const char *filename,
				      gpointer    data);

GtkFileFilter *gtk_file_filter_new_pattern   (const char            *pattern);
GtkFileFilter *gtk_file_filter_new_mime_type (const char            *mime_type);
GtkFileFilter *gtk_file_filter_new_callback  (GtkFileFilterFunction *func,
					      gpointer               data,
					      GDestroyNotify         notify);
#endif

G_END_DECLS

#endif /* __GTK_FILE_CHOOSER_H__ */
