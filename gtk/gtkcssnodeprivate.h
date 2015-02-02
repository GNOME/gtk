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

#define GTK_TYPE_CSS_NODE           (gtk_css_node_get_type ())
#define GTK_CSS_NODE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_NODE, GtkCssNode))
#define GTK_CSS_NODE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_NODE, GtkCssNodeClass))
#define GTK_IS_CSS_NODE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_NODE))
#define GTK_IS_CSS_NODE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_NODE))
#define GTK_CSS_NODE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_NODE, GtkCssNodeClass))

typedef struct _GtkCssNode           GtkCssNode;
typedef struct _GtkCssNodeClass      GtkCssNodeClass;

struct _GtkCssNode
{
  GObject object;

  GtkCssNode *parent;
  GtkCssNode *previous_sibling;
  GtkCssNode *next_sibling;
  GtkCssNode *first_child;
  GtkCssNode *last_child;
  guint n_children;

  GtkCssNodeDeclaration *decl;
  GtkCssStyle           *style;

  guint                  invalid :1;    /* set if node or a child is invalid */
};

struct _GtkCssNodeClass
{
  GObjectClass object_class;

  GtkWidgetPath *       (* create_widget_path)          (GtkCssNode            *cssnode);
  const GtkWidgetPath * (* get_widget_path)             (GtkCssNode            *cssnode);
  GtkStyleProviderPrivate *(* get_style_provider)       (GtkCssNode            *cssnode);
  void                  (* invalidate)                  (GtkCssNode            *cssnode,
                                                         GtkCssChange           change);
  void                  (* set_invalid)                 (GtkCssNode            *node,
                                                         gboolean               invalid);
  GtkBitmask *          (* validate)                    (GtkCssNode            *cssnode,
                                                         gint64                 timestamp,
                                                         GtkCssChange           change,
                                                         const GtkBitmask      *parent_changes);
};

GType                   gtk_css_node_get_type           (void) G_GNUC_CONST;

void                    gtk_css_node_set_parent         (GtkCssNode            *cssnode,
                                                         GtkCssNode            *parent);
GtkCssNode *            gtk_css_node_get_parent         (GtkCssNode            *cssnode);
GtkCssNode *            gtk_css_node_get_first_child    (GtkCssNode            *cssnode);
GtkCssNode *            gtk_css_node_get_last_child     (GtkCssNode            *cssnode);
GtkCssNode *            gtk_css_node_get_previous_sibling(GtkCssNode           *cssnode);
GtkCssNode *            gtk_css_node_get_next_sibling   (GtkCssNode            *cssnode);

void                    gtk_css_node_set_widget_type    (GtkCssNode            *cssnode,
                                                         GType                  widget_type);
GType                   gtk_css_node_get_widget_type    (GtkCssNode            *cssnode);
void                    gtk_css_node_set_id             (GtkCssNode            *cssnode,
                                                         const char            *id);
const char *            gtk_css_node_get_id             (GtkCssNode            *cssnode);
void                    gtk_css_node_set_state          (GtkCssNode            *cssnode,
                                                         GtkStateFlags          state_flags);
GtkStateFlags           gtk_css_node_get_state          (GtkCssNode            *cssnode);
void                    gtk_css_node_set_junction_sides (GtkCssNode            *cssnode,
                                                         GtkJunctionSides       junction_sides);
GtkJunctionSides        gtk_css_node_get_junction_sides (GtkCssNode            *cssnode);
void                    gtk_css_node_add_class          (GtkCssNode            *cssnode,
                                                         GQuark                 style_class);
void                    gtk_css_node_remove_class       (GtkCssNode            *cssnode,
                                                         GQuark                 style_class);
gboolean                gtk_css_node_has_class          (GtkCssNode            *cssnode,
                                                         GQuark                 style_class);
GList *                 gtk_css_node_list_classes       (GtkCssNode            *cssnode);
void                    gtk_css_node_add_region         (GtkCssNode            *cssnode,
                                                         GQuark                 region,
                                                         GtkRegionFlags         flags);
void                    gtk_css_node_remove_region      (GtkCssNode            *cssnode,
                                                         GQuark                 region);
gboolean                gtk_css_node_has_region         (GtkCssNode            *cssnode,
                                                         GQuark                 region,
                                                         GtkRegionFlags        *out_flags);
GList *                 gtk_css_node_list_regions       (GtkCssNode            *cssnode);

const GtkCssNodeDeclaration *
                        gtk_css_node_get_declaration    (GtkCssNode            *cssnode);


GtkCssStyle *           gtk_css_node_get_style          (GtkCssNode            *cssnode);
void                    gtk_css_node_set_style          (GtkCssNode            *cssnode,
                                                         GtkCssStyle           *style);
GtkCssStyle *           gtk_css_node_create_style       (GtkCssNode            *cssnode);
GtkCssStyle *           gtk_css_node_update_style       (GtkCssNode            *cssnode,
                                                         GtkCssStyle           *style,
                                                         const GtkBitmask      *parent_changes);

void                    gtk_css_node_invalidate         (GtkCssNode            *cssnode,
                                                         GtkCssChange           change);
void                    gtk_css_node_validate           (GtkCssNode            *cssnode,
                                                         gint64                 timestamp,
                                                         GtkCssChange           change,
                                                         const GtkBitmask      *parent_changes);
void                    gtk_css_node_set_invalid        (GtkCssNode            *node,
                                                         gboolean               invalid);
GtkWidgetPath *         gtk_css_node_create_widget_path (GtkCssNode            *cssnode);
const GtkWidgetPath *   gtk_css_node_get_widget_path    (GtkCssNode            *cssnode);
GtkStyleProviderPrivate *gtk_css_node_get_style_provider(GtkCssNode            *cssnode);

G_END_DECLS

#endif /* __GTK_CSS_NODE_PRIVATE_H__ */
