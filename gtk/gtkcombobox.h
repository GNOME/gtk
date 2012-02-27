/* gtkcombobox.h
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_COMBO_BOX_H__
#define __GTK_COMBO_BOX_H__

#include <gtk/gtkbin.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeview.h>

G_BEGIN_DECLS

#define GTK_TYPE_COMBO_BOX             (gtk_combo_box_get_type ())
#define GTK_COMBO_BOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO_BOX, GtkComboBox))
#define GTK_COMBO_BOX_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_TYPE_COMBO_BOX, GtkComboBoxClass))
#define GTK_IS_COMBO_BOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COMBO_BOX))
#define GTK_IS_COMBO_BOX_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_TYPE_COMBO_BOX))
#define GTK_COMBO_BOX_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), GTK_TYPE_COMBO_BOX, GtkComboBoxClass))

typedef struct _GtkComboBox        GtkComboBox;
typedef struct _GtkComboBoxClass   GtkComboBoxClass;
typedef struct _GtkComboBoxPrivate GtkComboBoxPrivate;

struct _GtkComboBox
{
  GtkBin parent_instance;

  /*< private >*/
  GtkComboBoxPrivate *priv;
};

struct _GtkComboBoxClass
{
  GtkBinClass parent_class;

  /* signals */
  void     (* changed)           (GtkComboBox *combo_box);
  gchar   *(* format_entry_text) (GtkComboBox *combo_box,
				  const gchar *path);

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
};


/* construction */
GType         gtk_combo_box_get_type                 (void) G_GNUC_CONST;
GtkWidget    *gtk_combo_box_new                      (void);
GtkWidget    *gtk_combo_box_new_with_area            (GtkCellArea  *area);
GtkWidget    *gtk_combo_box_new_with_area_and_entry  (GtkCellArea  *area);
GtkWidget    *gtk_combo_box_new_with_entry           (void);
GtkWidget    *gtk_combo_box_new_with_model           (GtkTreeModel *model);
GtkWidget    *gtk_combo_box_new_with_model_and_entry (GtkTreeModel *model);

/* grids */
gint          gtk_combo_box_get_wrap_width         (GtkComboBox *combo_box);
void          gtk_combo_box_set_wrap_width         (GtkComboBox *combo_box,
                                                    gint         width);
gint          gtk_combo_box_get_row_span_column    (GtkComboBox *combo_box);
void          gtk_combo_box_set_row_span_column    (GtkComboBox *combo_box,
                                                    gint         row_span);
gint          gtk_combo_box_get_column_span_column (GtkComboBox *combo_box);
void          gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                                    gint         column_span);

gboolean      gtk_combo_box_get_add_tearoffs       (GtkComboBox *combo_box);
void          gtk_combo_box_set_add_tearoffs       (GtkComboBox *combo_box,
                                                    gboolean     add_tearoffs);

const gchar * gtk_combo_box_get_title              (GtkComboBox *combo_box);
void          gtk_combo_box_set_title              (GtkComboBox *combo_box,
                                                    const gchar *title);

gboolean      gtk_combo_box_get_focus_on_click     (GtkComboBox *combo);
void          gtk_combo_box_set_focus_on_click     (GtkComboBox *combo,
                                                    gboolean     focus_on_click);

/* get/set active item */
gint          gtk_combo_box_get_active       (GtkComboBox     *combo_box);
void          gtk_combo_box_set_active       (GtkComboBox     *combo_box,
                                              gint             index_);
gboolean      gtk_combo_box_get_active_iter  (GtkComboBox     *combo_box,
                                              GtkTreeIter     *iter);
void          gtk_combo_box_set_active_iter  (GtkComboBox     *combo_box,
                                              GtkTreeIter     *iter);

/* getters and setters */
void          gtk_combo_box_set_model        (GtkComboBox     *combo_box,
                                              GtkTreeModel    *model);
GtkTreeModel *gtk_combo_box_get_model        (GtkComboBox     *combo_box);

GtkTreeViewRowSeparatorFunc gtk_combo_box_get_row_separator_func (GtkComboBox                *combo_box);
void                        gtk_combo_box_set_row_separator_func (GtkComboBox                *combo_box,
                                                                  GtkTreeViewRowSeparatorFunc func,
                                                                  gpointer                    data,
                                                                  GDestroyNotify              destroy);

void               gtk_combo_box_set_button_sensitivity (GtkComboBox        *combo_box,
                                                         GtkSensitivityType  sensitivity);
GtkSensitivityType gtk_combo_box_get_button_sensitivity (GtkComboBox        *combo_box);

gboolean           gtk_combo_box_get_has_entry          (GtkComboBox        *combo_box);
void               gtk_combo_box_set_entry_text_column  (GtkComboBox        *combo_box,
                                                         gint                text_column);
gint               gtk_combo_box_get_entry_text_column  (GtkComboBox        *combo_box);

void               gtk_combo_box_set_popup_fixed_width  (GtkComboBox      *combo_box,
                                                         gboolean          fixed);
gboolean           gtk_combo_box_get_popup_fixed_width  (GtkComboBox      *combo_box);

/* programmatic control */
void          gtk_combo_box_popup            (GtkComboBox     *combo_box);
void          gtk_combo_box_popup_for_device (GtkComboBox     *combo_box,
                                              GdkDevice       *device);
void          gtk_combo_box_popdown          (GtkComboBox     *combo_box);
AtkObject*    gtk_combo_box_get_popup_accessible (GtkComboBox *combo_box);

gint          gtk_combo_box_get_id_column        (GtkComboBox *combo_box);
void          gtk_combo_box_set_id_column        (GtkComboBox *combo_box,
                                                  gint         id_column);
const gchar * gtk_combo_box_get_active_id        (GtkComboBox *combo_box);
gboolean      gtk_combo_box_set_active_id        (GtkComboBox *combo_box,
                                                  const gchar *active_id);

G_END_DECLS

#endif /* __GTK_COMBO_BOX_H__ */
