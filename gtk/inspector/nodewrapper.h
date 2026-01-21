/*
 * Copyright (c) 2025 Benjamin Otte
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

#pragma once

#include <gtk.h>

#include "gsk/gskdebugnodeprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_INSPECTOR_NODE_WRAPPER gtk_inspector_node_wrapper_get_type ()

G_DECLARE_FINAL_TYPE (GtkInspectorNodeWrapper, gtk_inspector_node_wrapper, GTK, INSPECTOR_NODE_WRAPPER, GObject)


GtkInspectorNodeWrapper *
                        gtk_inspector_node_wrapper_new                  (GskRenderNode                  *node,
                                                                         GskRenderNode                  *perf_node,
                                                                         GskRenderNode                  *draw_node,
                                                                         const char                     *role);

GskRenderNode *         gtk_inspector_node_wrapper_get_node             (GtkInspectorNodeWrapper        *self);
GskRenderNode *         gtk_inspector_node_wrapper_get_draw_node        (GtkInspectorNodeWrapper        *self);
GskRenderNode *         gtk_inspector_node_wrapper_get_profile_node     (GtkInspectorNodeWrapper        *self);
const GskDebugProfile * gtk_inspector_node_wrapper_get_profile          (GtkInspectorNodeWrapper        *self);
const char *            gtk_inspector_node_wrapper_get_role             (GtkInspectorNodeWrapper        *self);

GskRenderNode *         gtk_inspector_node_wrapper_create_heat_map      (GtkInspectorNodeWrapper        *self);
GListModel *            gtk_inspector_node_wrapper_create_children_model(GtkInspectorNodeWrapper        *self);

G_END_DECLS
