/* gtkcombobox.h
 * Copyright (C) 2002, 2003  Kristian Rietveld <kris@gtk.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

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
#define GTK_COMBO_BOX_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GTK_TYPE_COMBO_BOX, GtkComboBoxPrivate))

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
  void     (* changed)          (GtkComboBox *combo_box);
};


/* construction */
GType         gtk_combo_box_get_type         (void);
GtkWidget    *gtk_combo_box_new              (GtkTreeModel    *model);

/* manipulate list of cell renderers */
void          gtk_combo_box_pack_start       (GtkComboBox     *combo_box,
                                              GtkCellRenderer *cell,
                                              gboolean         expand);
void          gtk_combo_box_pack_end         (GtkComboBox     *combo_box,
                                              GtkCellRenderer *cell,
                                              gboolean         expand);
void          gtk_combo_box_set_attributes   (GtkComboBox     *combo_box,
                                              GtkCellRenderer *cell,
                                              ...);
void          gtk_combo_box_clear            (GtkComboBox     *combo_box);

/* grids */
void          gtk_combo_box_set_wrap_width         (GtkComboBox *combo_box,
                                                    gint         width);
void          gtk_combo_box_set_row_span_column    (GtkComboBox *combo_box,
                                                    gint         row_span);
void          gtk_combo_box_set_column_span_column (GtkComboBox *combo_box,
                                                    gint         column_span);

/* get/set active item */
gint          gtk_combo_box_get_active       (GtkComboBox     *combo_box);
void          gtk_combo_box_set_active       (GtkComboBox     *combo_box,
                                              gint             index);

/* getters and setters */
GtkTreeModel *gtk_combo_box_get_model        (GtkComboBox     *combo_box);

/* convenience -- text */
GtkWidget    *gtk_combo_box_new_text         (void);
void          gtk_combo_box_append_text      (GtkComboBox     *combo_box,
                                              const gchar     *text);
void          gtk_combo_box_insert_text      (GtkComboBox     *combo_box,
                                              gint             position,
                                              const gchar     *text);
void          gtk_combo_box_prepend_text     (GtkComboBox     *combo_box,
                                              const gchar     *text);

G_END_DECLS

#endif /* __GTK_COMBO_BOX_H__ */
