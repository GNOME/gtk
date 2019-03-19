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
#include "gtkcssnodestylecacheprivate.h"
#include "gtkcssstylechangeprivate.h"
#include "gtkbitmaskprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkstylecontext.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_NODE           (gtk_css_node_get_type ())
#define GTK_CSS_NODE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_NODE, GtkCssNode))
#define GTK_CSS_NODE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_NODE, GtkCssNodeClass))
#define GTK_IS_CSS_NODE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_NODE))
#define GTK_IS_CSS_NODE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_NODE))
#define GTK_CSS_NODE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_NODE, GtkCssNodeClass))

typedef struct _GtkCssNodeClass         GtkCssNodeClass;

struct _GtkCssNode
{
  GObject object;

  GtkCssNode *parent;
  GtkCssNode *previous_sibling;
  GtkCssNode *next_sibling;
  GtkCssNode *first_child;
  GtkCssNode *last_child;

  GtkCssNodeDeclaration *decl;
  GtkCssStyle           *style;
  GtkCssNodeStyleCache  *cache;                 /* cache for children to look up styles */

  GtkCssChange           pending_changes;       /* changes that accumulated since the style was last computed */

  guint                  visible :1;            /* node will be skipped when validating or computing styles */
  guint                  invalid :1;            /* node or a child needs to be validated (even if just for animation) */
  guint                  needs_propagation :1;  /* children have state changes that need to be propagated to their siblings */
  /* Two invariants hold for this variable:
   * style_is_invalid == TRUE  =>  next_sibling->style_is_invalid == TRUE
   * style_is_invalid == FALSE =>  first_child->style_is_invalid == TRUE
   * So if a valid style is computed, one has to previously ensure that the parent's and the previous sibling's style
   * are valid. This allows both validation and invalidation to run in O(nodes-in-tree) */
  guint                  style_is_invalid :1;   /* the style needs to be recomputed */
};

struct _GtkCssNodeClass
{
  GObjectClass object_class;

  void                  (* node_added)                  (GtkCssNode            *cssnode,
                                                         GtkCssNode            *child,
                                                         GtkCssNode            *previous);
  void                  (* node_removed)                (GtkCssNode            *cssnode,
                                                         GtkCssNode            *child,
                                                         GtkCssNode            *previous);
  void                  (* style_changed)               (GtkCssNode            *cssnode,
                                                         GtkCssStyleChange     *style_change);

  gboolean              (* init_matcher)                (GtkCssNode            *cssnode,
                                                         GtkCssMatcher         *matcher);
  GtkWidgetPath *       (* create_widget_path)          (GtkCssNode            *cssnode);
  const GtkWidgetPath * (* get_widget_path)             (GtkCssNode            *cssnode);
  /* get style provider to use or NULL to use parent's */
  GtkStyleProvider *    (* get_style_provider)          (GtkCssNode            *cssnode);
  /* get frame clock or NULL (only relevant for root node) */
  GdkFrameClock *       (* get_frame_clock)             (GtkCssNode            *cssnode);
  GtkCssStyle *         (* update_style)                (GtkCssNode            *cssnode,
                                                         GtkCssChange           pending_changes,
                                                         gint64                 timestamp,
                                                         GtkCssStyle           *old_style);
  void                  (* invalidate)                  (GtkCssNode            *node);
  void                  (* queue_validate)              (GtkCssNode            *node);
  void                  (* dequeue_validate)            (GtkCssNode            *node);
  void                  (* validate)                    (GtkCssNode            *node);
};

GType                   gtk_css_node_get_type           (void) G_GNUC_CONST;

GtkCssNode *            gtk_css_node_new                (void);

void                    gtk_css_node_set_parent         (GtkCssNode            *cssnode,
                                                         GtkCssNode            *parent);
void                    gtk_css_node_insert_after       (GtkCssNode            *parent,
                                                         GtkCssNode            *cssnode,
                                                         GtkCssNode            *previous_sibling);
void                    gtk_css_node_insert_before      (GtkCssNode            *parent,
                                                         GtkCssNode            *cssnode,
                                                         GtkCssNode            *next_sibling);

GtkCssNode *            gtk_css_node_get_parent         (GtkCssNode            *cssnode) G_GNUC_PURE;
GtkCssNode *            gtk_css_node_get_first_child    (GtkCssNode            *cssnode) G_GNUC_PURE;
GtkCssNode *            gtk_css_node_get_last_child     (GtkCssNode            *cssnode) G_GNUC_PURE;
GtkCssNode *            gtk_css_node_get_previous_sibling(GtkCssNode           *cssnode) G_GNUC_PURE;
GtkCssNode *            gtk_css_node_get_next_sibling   (GtkCssNode            *cssnode) G_GNUC_PURE;

void                    gtk_css_node_set_visible        (GtkCssNode            *cssnode,
                                                         gboolean               visible);
gboolean                gtk_css_node_get_visible        (GtkCssNode            *cssnode) G_GNUC_PURE;

void                    gtk_css_node_set_name           (GtkCssNode            *cssnode,
                                                         /*interned*/const char*name);
/*interned*/const char *gtk_css_node_get_name           (GtkCssNode            *cssnode) G_GNUC_PURE;
void                    gtk_css_node_set_widget_type    (GtkCssNode            *cssnode,
                                                         GType                  widget_type);
GType                   gtk_css_node_get_widget_type    (GtkCssNode            *cssnode) G_GNUC_PURE;
void                    gtk_css_node_set_id             (GtkCssNode            *cssnode,
                                                         /*interned*/const char*id);
/*interned*/const char *gtk_css_node_get_id             (GtkCssNode            *cssnode) G_GNUC_PURE;
void                    gtk_css_node_set_state          (GtkCssNode            *cssnode,
                                                         GtkStateFlags          state_flags);
GtkStateFlags           gtk_css_node_get_state          (GtkCssNode            *cssnode) G_GNUC_PURE;
void                    gtk_css_node_set_classes        (GtkCssNode            *cssnode,
                                                         const char           **classes);
char **                 gtk_css_node_get_classes        (GtkCssNode            *cssnode);
void                    gtk_css_node_add_class          (GtkCssNode            *cssnode,
                                                         GQuark                 style_class);
void                    gtk_css_node_remove_class       (GtkCssNode            *cssnode,
                                                         GQuark                 style_class);
gboolean                gtk_css_node_has_class          (GtkCssNode            *cssnode,
                                                         GQuark                 style_class) G_GNUC_PURE;
const GQuark *          gtk_css_node_list_classes       (GtkCssNode            *cssnode,
                                                         guint                 *n_classes);

const GtkCssNodeDeclaration *
                        gtk_css_node_get_declaration    (GtkCssNode            *cssnode) G_GNUC_PURE;
GtkCssStyle *           gtk_css_node_get_style          (GtkCssNode            *cssnode) G_GNUC_PURE;


void                    gtk_css_node_invalidate_style_provider
                                                        (GtkCssNode            *cssnode);
void                    gtk_css_node_invalidate_frame_clock
                                                        (GtkCssNode            *cssnode,
                                                         gboolean               just_timestamp);
void                    gtk_css_node_invalidate         (GtkCssNode            *cssnode,
                                                         GtkCssChange           change);
void                    gtk_css_node_validate           (GtkCssNode            *cssnode);

gboolean                gtk_css_node_init_matcher       (GtkCssNode            *cssnode,
                                                         GtkCssMatcher         *matcher);
GtkWidgetPath *         gtk_css_node_create_widget_path (GtkCssNode            *cssnode);
const GtkWidgetPath *   gtk_css_node_get_widget_path    (GtkCssNode            *cssnode) G_GNUC_PURE;
GtkStyleProvider *      gtk_css_node_get_style_provider (GtkCssNode            *cssnode) G_GNUC_PURE;

void                    gtk_css_node_print              (GtkCssNode                *cssnode,
                                                         GtkStyleContextPrintFlags  flags,
                                                         GString                   *string,
                                                         guint                      indent);

G_END_DECLS

#endif /* __GTK_CSS_NODE_PRIVATE_H__ */
