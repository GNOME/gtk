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
 * License along with this library; if not, write to the
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

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
};


/* construction */
GType         gtk_combo_box_get_type         (void);
GtkWidget    *gtk_combo_box_new              (void);
GtkWidget    *gtk_combo_box_new_with_model   (GtkTreeModel    *model);

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
                                              gint             index_);
gboolean      gtk_combo_box_get_active_iter  (GtkComboBox     *combo_box,
                                              GtkTreeIter     *iter);
void          gtk_combo_box_set_active_iter  (GtkComboBox     *combo_box,
                                              GtkTreeIter     *iter);

/* getters and setters */
void          gtk_combo_box_set_model        (GtkComboBox     *combo_box,
                                              GtkTreeModel    *model);
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
void          gtk_combo_box_remove_text      (GtkComboBox     *combo_box,
                                              gint             position);

/* programmatic control */
void          gtk_combo_box_popup            (GtkComboBox     *combo_box);
void          gtk_combo_box_popdown          (GtkComboBox     *combo_box);

G_END_DECLS

#endif /* __GTK_COMBO_BOX_H__ */
