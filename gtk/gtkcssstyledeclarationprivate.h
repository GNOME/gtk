/*
 * Copyright © 2016 Red Hat Inc.
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

#ifndef __GTK_CSS_STYLE_DECLARATION_PRIVATE_H__
#define __GTK_CSS_STYLE_DECLARATION_PRIVATE_H__

#include "gtk/gtkcssruleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STYLE_DECLARATION           (gtk_css_style_declaration_get_type ())
#define GTK_CSS_STYLE_DECLARATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STYLE_DECLARATION, GtkCssStyleDeclaration))
#define GTK_CSS_STYLE_DECLARATION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STYLE_DECLARATION, GtkCssStyleDeclarationClass))
#define GTK_IS_CSS_STYLE_DECLARATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STYLE_DECLARATION))
#define GTK_IS_CSS_STYLE_DECLARATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STYLE_DECLARATION))
#define GTK_CSS_STYLE_DECLARATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STYLE_DECLARATION, GtkCssStyleDeclarationClass))

typedef struct _GtkCssStyleDeclaration           GtkCssStyleDeclaration;
typedef struct _GtkCssStyleDeclarationClass      GtkCssStyleDeclarationClass;

struct _GtkCssStyleDeclaration
{
  GObject parent;
};

struct _GtkCssStyleDeclarationClass
{
  GObjectClass parent_class;
};

GType                   gtk_css_style_declaration_get_type              (void) G_GNUC_CONST;

GtkCssStyleDeclaration *gtk_css_style_declaration_new                   (GtkCssRule             *parent_rule);

void                    gtk_css_style_declaration_parse                 (GtkCssStyleDeclaration *style,
                                                                         GtkCssTokenSource      *source);

/* GtkCssStyleDeclaration DOM */
void                    gtk_css_style_declaration_print_css_text        (GtkCssStyleDeclaration *declaration,
                                                                         GString                *string);
char *                  gtk_css_style_declaration_get_css_text          (GtkCssStyleDeclaration *declaration);
gsize                   gtk_css_style_declaration_get_length            (GtkCssStyleDeclaration *declaration);
char *                  gtk_css_style_declaration_get_item              (GtkCssStyleDeclaration *declaration,
                                                                         gssize                  size);


G_END_DECLS

#endif /* __GTK_CSS_STYLE_DECLARATION_PRIVATE_H__ */
