/* gtkcomboboxentry.h
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

#ifndef __GTK_COMBO_BOX_ENTRY_H__
#define __GTK_COMBO_BOX_ENTRY_H__

#include <gtk/gtkcombobox.h>
#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

#define GTK_TYPE_COMBO_BOX_ENTRY             (gtk_combo_box_entry_get_type ())
#define GTK_COMBO_BOX_ENTRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COMBO_BOX_ENTRY, GtkComboBoxEntry))
#define GTK_COMBO_BOX_ENTRY_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST ((vtable), GTK_TYPE_COMBO_BOX_ENTRY, GtkComboBoxEntryClass))
#define GTK_IS_COMBO_BOX_ENTRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COMBO_BOX_ENTRY))
#define GTK_IS_COMBO_BOX_ENTRY_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE ((vtable), GTK_TYPE_COMBO_BOX_ENTRY))
#define GTK_COMBO_BOX_ENTRY_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS ((inst), GTK_TYPE_COMBO_BOX_ENTRY, GtkComboBoxEntryClass))

typedef struct _GtkComboBoxEntry             GtkComboBoxEntry;
typedef struct _GtkComboBoxEntryClass        GtkComboBoxEntryClass;
typedef struct _GtkComboBoxEntryPrivate      GtkComboBoxEntryPrivate;

struct _GtkComboBoxEntry
{
  GtkComboBox parent_instance;

  /*< private >*/
  GtkComboBoxEntryPrivate *priv;
};

struct _GtkComboBoxEntryClass
{
  GtkComboBoxClass parent_class;
};


GType       gtk_combo_box_entry_get_type        (void);
GtkWidget  *gtk_combo_box_entry_new             (GtkTreeModel     *model,
                                                 gint              text_column);

gint        gtk_combo_box_entry_get_text_column (GtkComboBoxEntry *entry_box);


G_END_DECLS

#endif /* __GTK_COMBO_BOX_ENTRY_H__ */
