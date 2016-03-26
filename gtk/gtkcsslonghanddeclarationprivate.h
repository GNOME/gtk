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

#ifndef __GTK_CSS_LONGHAND_DECLARATION_PRIVATE_H__
#define __GTK_CSS_LONGHAND_DECLARATION_PRIVATE_H__

#include "gtk/gtkcssdeclarationprivate.h"

#include "gtk/gtkcssstylepropertyprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_LONGHAND_DECLARATION           (gtk_css_longhand_declaration_get_type ())
#define GTK_CSS_LONGHAND_DECLARATION(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_LONGHAND_DECLARATION, GtkCssLonghandDeclaration))
#define GTK_CSS_LONGHAND_DECLARATION_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_LONGHAND_DECLARATION, GtkCssLonghandDeclarationClass))
#define GTK_IS_CSS_LONGHAND_DECLARATION(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_LONGHAND_DECLARATION))
#define GTK_IS_CSS_LONGHAND_DECLARATION_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_LONGHAND_DECLARATION))
#define GTK_CSS_LONGHAND_DECLARATION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_LONGHAND_DECLARATION, GtkCssLonghandDeclarationClass))

typedef struct _GtkCssLonghandDeclaration           GtkCssLonghandDeclaration;
typedef struct _GtkCssLonghandDeclarationClass      GtkCssLonghandDeclarationClass;

struct _GtkCssLonghandDeclaration
{
  GtkCssDeclaration parent;
};

struct _GtkCssLonghandDeclarationClass
{
  GtkCssDeclarationClass parent_class;
};

GType                   gtk_css_longhand_declaration_get_type           (void) G_GNUC_CONST;
  
GtkCssDeclaration *     gtk_css_longhand_declaration_new_parse          (GtkCssStyleDeclaration         *style,
                                                                         GtkCssTokenSource              *source);

guint                   gtk_css_longhand_declaration_get_id             (GtkCssLonghandDeclaration      *decl);
GtkCssStyleProperty *   gtk_css_longhand_declaration_get_property       (GtkCssLonghandDeclaration      *decl);
GtkCssValue *           gtk_css_longhand_declaration_get_value          (GtkCssLonghandDeclaration      *decl);


G_END_DECLS

#endif /* __GTK_CSS_LONGHAND_DECLARATION_PRIVATE_H__ */
