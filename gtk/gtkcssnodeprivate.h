/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_NODE_PRIVATE_H__
#define __GTK_CSS_NODE_PRIVATE_H__

#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssstyleprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssNode GtkCssNode;

struct _GtkCssNode
{
  GtkCssNodeDeclaration *decl;
  GtkCssNode            *parent;
  GtkCssStyle           *style;
};

GtkCssNode *            gtk_css_node_new                (void);

void                    gtk_css_node_free               (GtkCssNode            *cssnode);

GtkCssNode *            gtk_css_node_get_parent         (GtkCssNode            *cssnode);

GtkCssStyle *           gtk_css_node_get_style          (GtkCssNode            *cssnode);
void                    gtk_css_node_set_style          (GtkCssNode            *cssnode,
                                                         GtkCssStyle           *style);

G_END_DECLS

#endif /* __GTK_CSS_NODE_PRIVATE_H__ */
