/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#ifndef __GTK_CONCAT_MODEL_H__
#define __GTK_CONCAT_MODEL_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define GTK_TYPE_CONCAT_MODEL         (gtk_concat_model_get_type ())
#define GTK_CONCAT_MODEL(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_CONCAT_MODEL, GtkConcatModel))
#define GTK_CONCAT_MODEL_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_CONCAT_MODEL, GtkConcatModelClass))
#define GTK_IS_CONCAT_MODEL(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_CONCAT_MODEL))
#define GTK_IS_CONCAT_MODEL_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_CONCAT_MODEL))
#define GTK_CONCAT_MODEL_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_CONCAT_MODEL, GtkConcatModelClass))

typedef struct _GtkConcatModel GtkConcatModel;
typedef struct _GtkConcatModelClass GtkConcatModelClass;

GType                   gtk_concat_model_get_type                (void) G_GNUC_CONST;

GtkConcatModel *        gtk_concat_model_new                     (GType                  item_type);

void                    gtk_concat_model_append                  (GtkConcatModel        *self,
                                                                  GListModel            *model);
void                    gtk_concat_model_remove                  (GtkConcatModel        *self,
                                                                  GListModel            *model);

GListModel *            gtk_concat_model_get_model_for_item      (GtkConcatModel        *self,
                                                                  guint                  position);

G_END_DECLS

#endif /* __GTK_CONCAT_MODEL_H__ */
