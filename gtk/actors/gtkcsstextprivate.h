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

#ifndef __GTK_CSS_TEXT_PRIVATE_H__
#define __GTK_CSS_TEXT_PRIVATE_H__

#include <gtk/actors/gtkcssactorprivate.h>

G_BEGIN_DECLS

#define GTK_TYPE_CSS_TEXT           (_gtk_css_text_get_type ())
#define GTK_CSS_TEXT(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_TEXT, GtkCssText))
#define GTK_CSS_TEXT_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_TEXT, GtkCssTextClass))
#define GTK_IS_CSS_TEXT(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_TEXT))
#define GTK_IS_CSS_TEXT_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_TEXT))
#define GTK_CSS_TEXT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_TEXT, GtkCssTextClass))

typedef struct _GtkCssText           GtkCssText;
typedef struct _GtkCssTextPrivate    GtkCssTextPrivate;
typedef struct _GtkCssTextClass      GtkCssTextClass;
typedef struct _GtkCssTextIter       GtkCssTextIter;

struct _GtkCssText
{
  GtkCssActor          parent;

  GtkCssTextPrivate    *priv;
};

struct _GtkCssTextClass
{
  GtkCssActorClass     parent_class;
};

GType                           _gtk_css_text_get_type                          (void) G_GNUC_CONST;

GtkActor *                      _gtk_css_text_new                               (void);

const char *                    _gtk_css_text_get_text                          (GtkCssText                   *self);
void                            _gtk_css_text_set_text                          (GtkCssText                   *self,
                                                                                 const char                   *text);
PangoEllipsizeMode              _gtk_css_text_get_ellipsize                     (GtkCssText                   *self);
void                            _gtk_css_text_set_ellipsize                     (GtkCssText                   *self,
					                                         PangoEllipsizeMode            ellipsize);
PangoAlignment                  _gtk_css_text_get_text_alignment                (GtkCssText                   *self);
void                            _gtk_css_text_set_text_alignment                (GtkCssText                   *self,
						                                 PangoAlignment              alignment);

G_END_DECLS

#endif /* __GTK_CSS_TEXT_PRIVATE_H__ */
