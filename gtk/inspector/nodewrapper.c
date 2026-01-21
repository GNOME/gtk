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

#include "config.h"

#include "nodewrapper.h"

#include "gsk/gskdebugnodeprivate.h"
#include "gsk/gskrendernodeprivate.h"

#include <glib/gi18n-lib.h>

struct _GtkInspectorNodeWrapper
{
  GObject parent;

  GskRenderNode *node;
  GskRenderNode *profile_node;
  GskRenderNode *draw_node;
  char *role;
};

struct _GtkInspectorNodeWrapperClass
{
  GObjectClass parent;
};

enum
{
  PROP_0,
  PROP_DRAW_NODE,
  PROP_NODE,
  PROP_PROFILE_NODE,
  PROP_ROLE,

  N_PROPS,
};

static GParamSpec *props[N_PROPS] = { NULL, };


G_DEFINE_TYPE (GtkInspectorNodeWrapper, gtk_inspector_node_wrapper, G_TYPE_OBJECT)

static void
gtk_inspector_node_wrapper_get_property (GObject    *object,
                                         guint       param_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  switch (param_id)
    {
    case PROP_DRAW_NODE:
      g_value_set_boxed (value, self->draw_node);
      break;

    case PROP_NODE:
      g_value_set_boxed (value, self->node);
      break;

    case PROP_PROFILE_NODE:
      g_value_set_boxed (value, self->profile_node);
      break;

    case PROP_ROLE:
      g_value_set_string (value, self->role);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_node_wrapper_set_property (GObject      *object,
                                         guint         param_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  switch (param_id)
    {
    case PROP_DRAW_NODE:
      if (g_value_get_pointer (value))
        self->draw_node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_NODE:
      self->node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_PROFILE_NODE:
      if (g_value_get_pointer (value))
        self->profile_node = gsk_render_node_ref (g_value_get_pointer (value));
      break;

    case PROP_ROLE:
      self->role = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

static void
gtk_inspector_node_wrapper_dispose (GObject *object)
{
  GtkInspectorNodeWrapper *self = GTK_INSPECTOR_NODE_WRAPPER (object);

  g_clear_pointer (&self->node, gsk_render_node_unref);
  g_clear_pointer (&self->profile_node, gsk_render_node_unref);
  g_clear_pointer (&self->draw_node, gsk_render_node_unref);

  G_OBJECT_CLASS (gtk_inspector_node_wrapper_parent_class)->dispose (object);
}

static void
gtk_inspector_node_wrapper_class_init (GtkInspectorNodeWrapperClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_inspector_node_wrapper_dispose;
  object_class->get_property = gtk_inspector_node_wrapper_get_property;
  object_class->set_property = gtk_inspector_node_wrapper_set_property;

  props[PROP_DRAW_NODE] =
    g_param_spec_pointer ("draw-node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_NODE] =
    g_param_spec_pointer ("node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_PROFILE_NODE] =
    g_param_spec_pointer ("profile-node", NULL, NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  props[PROP_ROLE] =
    g_param_spec_string ("role", NULL, NULL,
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT);

  g_object_class_install_properties (object_class, N_PROPS, props);
}

static void
gtk_inspector_node_wrapper_init (GtkInspectorNodeWrapper *vis)
{
}

GtkInspectorNodeWrapper *
gtk_inspector_node_wrapper_new (GskRenderNode *node,
                                GskRenderNode *profile_node,
                                GskRenderNode *draw_node,
                                const char    *role)
{
  return g_object_new (GTK_TYPE_INSPECTOR_NODE_WRAPPER,
                       "node", node,
                       "profile-node", profile_node,
                       "draw-node", draw_node,
                       "role", role,
                       NULL);
}

GskRenderNode *
gtk_inspector_node_wrapper_get_node (GtkInspectorNodeWrapper *self)
{
  return self->node;
}

GskRenderNode *
gtk_inspector_node_wrapper_get_profile_node (GtkInspectorNodeWrapper *self)
{
  return self->profile_node;
}

GskRenderNode *
gtk_inspector_node_wrapper_get_draw_node (GtkInspectorNodeWrapper *self)
{
  return self->draw_node;
}

const char *
gtk_inspector_node_wrapper_get_role (GtkInspectorNodeWrapper *self)
{
  return self->role;
}

static const char **
get_roles (GskRenderNodeType node_type)
{
  static const char *blend_node_roles[] = { "Bottom", "Top", NULL };
  static const char *mask_node_roles[] = { "Source", "Mask", NULL };
  static const char *cross_fade_node_roles[] = { "Start", "End", NULL };
  static const char *composite_node_roles[] = { "Child", "Mask", NULL };
  static const char *displacement_node_roles[] = { "Child", "Displacement", NULL };
  static const char *arithmetic_node_roles[] = { "First", "Second", NULL };

  switch (node_type)
    {
    case GSK_BLEND_NODE:
      return blend_node_roles;
    case GSK_MASK_NODE:
      return mask_node_roles;
    case GSK_CROSS_FADE_NODE:
      return cross_fade_node_roles;
    case GSK_COMPOSITE_NODE:
      return composite_node_roles;
    case GSK_DISPLACEMENT_NODE:
      return displacement_node_roles;
    case GSK_ARITHMETIC_NODE:
      return arithmetic_node_roles;
    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SHADOW_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_COPY_NODE:
    case GSK_PASTE_NODE:
    case GSK_DEBUG_NODE:
    case GSK_COLOR_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_NOT_A_RENDER_NODE:
    default:
      return NULL;
    }
};

GListModel *
gtk_inspector_node_wrapper_create_children_model (GtkInspectorNodeWrapper *self)
{
  GskRenderNode **children, **draw_children, **profile_children;
  gsize i, n_children, n_draw_children, n_profile_children;
  GListStore *store;
  const char **roles;

  children = gsk_render_node_get_children (self->node, &n_children);

  if (n_children == 0)
    return NULL;

  store = g_list_store_new (GTK_TYPE_INSPECTOR_NODE_WRAPPER);

  if (self->draw_node)
    {
      if (gsk_render_node_get_node_type (self->node) == GSK_COPY_NODE)
        {
          draw_children = &self->draw_node;
          n_draw_children = 1;
        }
      else if (gsk_render_node_get_node_type (self->node) == GSK_PASTE_NODE)
        {
          draw_children = NULL;
          n_draw_children = 0;
        }
      else
        {
          draw_children = gsk_render_node_get_children (self->draw_node, &n_draw_children);
        }
    }
  else
    {
      draw_children = NULL;
      n_draw_children = 0;
    }

  if (self->profile_node)
    {
      if (gsk_render_node_get_node_type (self->profile_node) == GSK_DEBUG_NODE &&
          gsk_debug_node_get_profile (self->profile_node))
        {
          profile_children = gsk_render_node_get_children (gsk_debug_node_get_child (self->profile_node), &n_profile_children);
        }
      else
        {
          profile_children = gsk_render_node_get_children (self->profile_node, &n_profile_children);
        }
      g_assert (n_profile_children == n_children);
    }
  else
    {
      profile_children = NULL;
      n_profile_children = 0;
    }

  roles = get_roles (gsk_render_node_get_node_type (self->node));

  for (i = 0; i < n_children; i++)
    {
      GtkInspectorNodeWrapper *child;

      child = gtk_inspector_node_wrapper_new (children[i],
                                              i < n_profile_children ? profile_children[i] : NULL,
                                              i < n_draw_children ? draw_children[i] : NULL,
                                              roles ? roles[i] : NULL);
      g_list_store_append (store, child);
      g_object_unref (child);
    }

  return G_LIST_MODEL (store);
}

