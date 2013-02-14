/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

#ifndef __GTK_SELECTION_H__
#define __GTK_SELECTION_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>
#include <gtk/gtktextiter.h>

G_BEGIN_DECLS

/**
 * GtkTargetList:
 *
 * A #GtkTargetList structure is a reference counted list
 * of #GtkTargetPair. It is used to represent the same
 * information as a table of #GtkTargetEntry, but in
 * an efficient form. This structure should be treated as
 * opaque.
 */
typedef struct _GtkTargetList  GtkTargetList;
typedef struct _GtkTargetEntry GtkTargetEntry;

#define GTK_TYPE_SELECTION_DATA (gtk_selection_data_get_type ())
#define GTK_TYPE_TARGET_LIST    (gtk_target_list_get_type ())

/**
 * GtkTargetEntry:
 * @target: a string representation of the target type
 * @flags: #GtkTargetFlags for DND
 * @info: an application-assigned integer ID which will
 *     get passed as a parameter to e.g the #GtkWidget::selection-get
 *     signal. It allows the application to identify the target
 *     type without extensive string compares.
 *
 * A #GtkTargetEntry structure represents a single type of
 * data than can be supplied for by a widget for a selection
 * or for supplied or received during drag-and-drop.
 */
struct _GtkTargetEntry
{
  gchar *target;
  guint  flags;
  guint  info;
};

GType          gtk_target_list_get_type  (void) G_GNUC_CONST;
GtkTargetList *gtk_target_list_new       (const GtkTargetEntry *targets,
                                          guint                 ntargets);
GtkTargetList *gtk_target_list_ref       (GtkTargetList  *list);
void           gtk_target_list_unref     (GtkTargetList  *list);
void           gtk_target_list_add       (GtkTargetList  *list,
                                          GdkAtom         target,
                                          guint           flags,
                                          guint           info);
void           gtk_target_list_add_text_targets      (GtkTargetList  *list,
                                                      guint           info);
void           gtk_target_list_add_rich_text_targets (GtkTargetList  *list,
                                                      guint           info,
                                                      gboolean        deserializable,
                                                      GtkTextBuffer  *buffer);
void           gtk_target_list_add_image_targets     (GtkTargetList  *list,
                                                      guint           info,
                                                      gboolean        writable);
void           gtk_target_list_add_uri_targets       (GtkTargetList  *list,
                                                      guint           info);
void           gtk_target_list_add_table (GtkTargetList        *list,
                                          const GtkTargetEntry *targets,
                                          guint                 ntargets);
void           gtk_target_list_remove    (GtkTargetList  *list,
                                          GdkAtom         target);
gboolean       gtk_target_list_find      (GtkTargetList  *list,
                                          GdkAtom         target,
                                          guint          *info);

GtkTargetEntry * gtk_target_table_new_from_list (GtkTargetList  *list,
                                                 gint           *n_targets);
void             gtk_target_table_free          (GtkTargetEntry *targets,
                                                 gint            n_targets);

gboolean gtk_selection_owner_set             (GtkWidget  *widget,
                                              GdkAtom     selection,
                                              guint32     time_);
gboolean gtk_selection_owner_set_for_display (GdkDisplay *display,
                                              GtkWidget  *widget,
                                              GdkAtom     selection,
                                              guint32     time_);

void     gtk_selection_add_target    (GtkWidget            *widget,
                                      GdkAtom               selection,
                                      GdkAtom               target,
                                      guint                 info);
void     gtk_selection_add_targets   (GtkWidget            *widget,
                                      GdkAtom               selection,
                                      const GtkTargetEntry *targets,
                                      guint                 ntargets);
void     gtk_selection_clear_targets (GtkWidget            *widget,
                                      GdkAtom               selection);
gboolean gtk_selection_convert       (GtkWidget            *widget,
                                      GdkAtom               selection,
                                      GdkAtom               target,
                                      guint32               time_);
void     gtk_selection_remove_all    (GtkWidget             *widget);

GdkAtom       gtk_selection_data_get_selection (const GtkSelectionData *selection_data);
GdkAtom       gtk_selection_data_get_target    (const GtkSelectionData *selection_data);
GdkAtom       gtk_selection_data_get_data_type (const GtkSelectionData *selection_data);
gint          gtk_selection_data_get_format    (const GtkSelectionData *selection_data);
const guchar *gtk_selection_data_get_data      (const GtkSelectionData *selection_data);
gint          gtk_selection_data_get_length    (const GtkSelectionData *selection_data);
const guchar *gtk_selection_data_get_data_with_length
                                               (const GtkSelectionData *selection_data,
                                                gint                   *length);

GdkDisplay   *gtk_selection_data_get_display   (const GtkSelectionData *selection_data);

void     gtk_selection_data_set      (GtkSelectionData     *selection_data,
                                      GdkAtom               type,
                                      gint                  format,
                                      const guchar         *data,
                                      gint                  length);
gboolean gtk_selection_data_set_text (GtkSelectionData     *selection_data,
                                      const gchar          *str,
                                      gint                  len);
guchar * gtk_selection_data_get_text (const GtkSelectionData     *selection_data);
gboolean gtk_selection_data_set_pixbuf   (GtkSelectionData  *selection_data,
                                          GdkPixbuf         *pixbuf);
GdkPixbuf *gtk_selection_data_get_pixbuf (const GtkSelectionData  *selection_data);
gboolean gtk_selection_data_set_uris (GtkSelectionData     *selection_data,
                                      gchar               **uris);
gchar  **gtk_selection_data_get_uris (const GtkSelectionData     *selection_data);

gboolean gtk_selection_data_get_targets          (const GtkSelectionData  *selection_data,
                                                  GdkAtom          **targets,
                                                  gint              *n_atoms);
gboolean gtk_selection_data_targets_include_text (const GtkSelectionData  *selection_data);
gboolean gtk_selection_data_targets_include_rich_text (const GtkSelectionData *selection_data,
                                                       GtkTextBuffer    *buffer);
gboolean gtk_selection_data_targets_include_image (const GtkSelectionData  *selection_data,
                                                   gboolean           writable);
gboolean gtk_selection_data_targets_include_uri  (const GtkSelectionData  *selection_data);
gboolean gtk_targets_include_text                (GdkAtom       *targets,
                                                  gint           n_targets);
gboolean gtk_targets_include_rich_text           (GdkAtom       *targets,
                                                  gint           n_targets,
                                                  GtkTextBuffer *buffer);
gboolean gtk_targets_include_image               (GdkAtom       *targets,
                                                  gint           n_targets,
                                                  gboolean       writable);
gboolean gtk_targets_include_uri                 (GdkAtom       *targets,
                                                  gint           n_targets);


GType             gtk_selection_data_get_type (void) G_GNUC_CONST;
GtkSelectionData *gtk_selection_data_copy     (const GtkSelectionData *data);
void              gtk_selection_data_free     (GtkSelectionData *data);

GType             gtk_target_entry_get_type    (void) G_GNUC_CONST;
GtkTargetEntry   *gtk_target_entry_new        (const gchar    *target,
                                               guint           flags,
                                               guint           info);
GtkTargetEntry   *gtk_target_entry_copy       (GtkTargetEntry *data);
void              gtk_target_entry_free       (GtkTargetEntry *data);

G_END_DECLS

#endif /* __GTK_SELECTION_H__ */
