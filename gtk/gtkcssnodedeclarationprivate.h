/*
 * Copyright © 2014 Benjamin Otte <otte@gnome.org>
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
 */

#ifndef __GTK_CSS_NODE_DECLARATION_PRIVATE_H__
#define __GTK_CSS_NODE_DECLARATION_PRIVATE_H__

#include "gtkenums.h"
#include "gtkwidgetpath.h"

G_BEGIN_DECLS

typedef struct _GtkCssNodeDeclaration GtkCssNodeDeclaration;


GtkCssNodeDeclaration * gtk_css_node_declaration_new                    (void);
GtkCssNodeDeclaration * gtk_css_node_declaration_ref                    (GtkCssNodeDeclaration         *decl);
void                    gtk_css_node_declaration_unref                  (GtkCssNodeDeclaration         *decl);

gboolean                gtk_css_node_declaration_set_junction_sides     (GtkCssNodeDeclaration        **decl,
                                                                         GtkJunctionSides               junction_sides);
GtkJunctionSides        gtk_css_node_declaration_get_junction_sides     (const GtkCssNodeDeclaration   *decl);
gboolean                gtk_css_node_declaration_set_state              (GtkCssNodeDeclaration        **decl,
                                                                         GtkStateFlags                  flags);
GtkStateFlags           gtk_css_node_declaration_get_state              (const GtkCssNodeDeclaration   *decl);

gboolean                gtk_css_node_declaration_add_class              (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                gtk_css_node_declaration_remove_class           (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         class_quark);
gboolean                gtk_css_node_declaration_has_class              (const GtkCssNodeDeclaration   *decl,
                                                                         GQuark                         class_quark);
GList *                 gtk_css_node_declaration_list_classes           (const GtkCssNodeDeclaration   *decl);

gboolean                gtk_css_node_declaration_add_region             (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         region_quark,
                                                                         GtkRegionFlags                 flags);
gboolean                gtk_css_node_declaration_remove_region          (GtkCssNodeDeclaration        **decl,
                                                                         GQuark                         region_quark);
gboolean                gtk_css_node_declaration_has_region             (const GtkCssNodeDeclaration   *decl,
                                                                         GQuark                         region_quark,
                                                                         GtkRegionFlags                *flags_return);
GList *                 gtk_css_node_declaration_list_regions           (const GtkCssNodeDeclaration   *decl);

guint                   gtk_css_node_declaration_hash                   (gconstpointer                  elem);
gboolean                gtk_css_node_declaration_equal                  (gconstpointer                  elem1,
                                                                         gconstpointer                  elem2);

void                    gtk_css_node_declaration_add_to_widget_path     (const GtkCssNodeDeclaration   *decl,
                                                                         GtkWidgetPath                 *path,
                                                                         guint                          pos);
G_END_DECLS

#endif /* __GTK_CSS_NODE_DECLARATION_PRIVATE_H__ */
