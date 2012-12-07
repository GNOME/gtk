/*
 * Copyright © 2012 Red Hat Inc.
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

#ifndef __GTK_CSS_BOX_PRIVATE_H__
#define __GTK_CSS_BOX_PRIVATE_H__

#include <gtk/actors/gtkcssactorprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_CSS_BOX           (_gtk_css_box_get_type ())
#define GTK_CSS_BOX(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_BOX, GtkCssBox))
#define GTK_CSS_BOX_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_BOX, GtkCssBoxClass))
#define GTK_IS_CSS_BOX(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_BOX))
#define GTK_IS_CSS_BOX_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_BOX))
#define GTK_CSS_BOX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_BOX, GtkCssBoxClass))

typedef struct _GtkCssBox           GtkCssBox;
typedef struct _GtkCssBoxPrivate    GtkCssBoxPrivate;
typedef struct _GtkCssBoxClass      GtkCssBoxClass;
typedef struct _GtkCssBoxIter       GtkCssBoxIter;

struct _GtkCssBox
{
  GtkCssActor          parent;

  GtkCssBoxPrivate    *priv;
};

struct _GtkCssBoxClass
{
  GtkCssActorClass     parent_class;
};

GType                           _gtk_css_box_get_type                           (void) G_GNUC_CONST;

GtkActor *                      _gtk_css_box_new                                (void);

void                            _gtk_css_box_set_state                          (GtkCssBox                  *self,
                                                                                 GtkStateFlags               state);
GtkStateFlags                   _gtk_css_box_get_state                          (GtkCssBox                  *self);
GtkStateFlags                   _gtk_css_box_get_effective_state                (GtkCssBox                  *self);

void                            _gtk_css_box_set_id                             (GtkCssBox                  *self,
                                                                                 const char                 *id);
const char *                    _gtk_css_box_get_id                             (GtkCssBox                  *self);
void                            _gtk_css_box_add_class                          (GtkCssBox                  *self,
                                                                                 const gchar                *class_name);
void                            _gtk_css_box_remove_class                       (GtkCssBox                  *self,
                                                                                 const gchar                *class_name);
gboolean                        _gtk_css_box_has_class                          (GtkCssBox                  *self,
                                                                                 const gchar                *class_name);

G_END_DECLS

#endif /* __GTK_CSS_BOX_PRIVATE_H__ */
