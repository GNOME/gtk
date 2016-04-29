/*
 * Copyright © 2015 Red Hat Inc.
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

#ifndef __GTK_BOX_GADGET_PRIVATE_H__
#define __GTK_BOX_GADGET_PRIVATE_H__

#include "gtk/gtkcssgadgetprivate.h"
#include "gtk/gtkenums.h"

G_BEGIN_DECLS

#define GTK_TYPE_BOX_GADGET           (gtk_box_gadget_get_type ())
#define GTK_BOX_GADGET(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_BOX_GADGET, GtkBoxGadget))
#define GTK_BOX_GADGET_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_BOX_GADGET, GtkBoxGadgetClass))
#define GTK_IS_BOX_GADGET(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_BOX_GADGET))
#define GTK_IS_BOX_GADGET_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_BOX_GADGET))
#define GTK_BOX_GADGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_BOX_GADGET, GtkBoxGadgetClass))

typedef struct _GtkBoxGadget           GtkBoxGadget;
typedef struct _GtkBoxGadgetClass      GtkBoxGadgetClass;

struct _GtkBoxGadget
{
  GtkCssGadget parent;
};

struct _GtkBoxGadgetClass
{
  GtkCssGadgetClass  parent_class;
};

GType                   gtk_box_gadget_get_type                 (void) G_GNUC_CONST;

GtkCssGadget *          gtk_box_gadget_new                      (const char             *name,
                                                                 GtkWidget              *owner,
                                                                 GtkCssGadget           *parent,
                                                                 GtkCssGadget           *next_sibling);
GtkCssGadget *          gtk_box_gadget_new_for_node             (GtkCssNode             *node,
                                                                 GtkWidget              *owner);

void                    gtk_box_gadget_set_orientation          (GtkBoxGadget           *gadget,
                                                                 GtkOrientation          orientation);
void                    gtk_box_gadget_set_draw_focus           (GtkBoxGadget           *gadget,
                                                                 gboolean                draw_focus);
void                    gtk_box_gadget_set_draw_reverse         (GtkBoxGadget           *gadget,
                                                                 gboolean                draw_reverse);
void                    gtk_box_gadget_set_allocate_reverse     (GtkBoxGadget           *gadget,
                                                                 gboolean                allocate_reverse);

void                    gtk_box_gadget_set_align_reverse        (GtkBoxGadget           *gadget,
                                                                 gboolean                align_reverse);
void                    gtk_box_gadget_insert_widget            (GtkBoxGadget           *gadget,
                                                                 int                     pos,
                                                                 GtkWidget              *widget);
void                    gtk_box_gadget_remove_widget            (GtkBoxGadget           *gadget,
                                                                 GtkWidget              *widget);
void                    gtk_box_gadget_insert_gadget            (GtkBoxGadget           *gadget,
                                                                 int                     pos,
                                                                 GtkCssGadget           *cssgadget,
                                                                 gboolean                expand,
                                                                 GtkAlign                align);
void                    gtk_box_gadget_insert_gadget_before     (GtkBoxGadget           *gadget,
                                                                 GtkCssGadget           *sibling,
                                                                 GtkCssGadget           *cssgadget,
                                                                 gboolean                expand,
                                                                 GtkAlign                align);
void                    gtk_box_gadget_insert_gadget_after      (GtkBoxGadget           *gadget,
                                                                 GtkCssGadget           *sibling,
                                                                 GtkCssGadget           *cssgadget,
                                                                 gboolean                expand,
                                                                 GtkAlign                align);

void                    gtk_box_gadget_remove_gadget            (GtkBoxGadget           *gadget,
                                                                 GtkCssGadget           *cssgadget);
void                    gtk_box_gadget_reverse_children         (GtkBoxGadget           *gadget);

void                    gtk_box_gadget_set_gadget_expand        (GtkBoxGadget           *gadget,
                                                                 GObject                *object,
                                                                 gboolean                expand);
void                    gtk_box_gadget_set_gadget_align         (GtkBoxGadget           *gadget,
                                                                 GObject                *object,
                                                                 GtkAlign                align);

G_END_DECLS

#endif /* __GTK_BOX_GADGET_PRIVATE_H__ */
