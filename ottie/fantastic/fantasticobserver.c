/*
 * Copyright © 2020 Benjamin Otte
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

#include "config.h"

#include "fantasticobserver.h"

#include "ottie/ottieobjectprivate.h"

#include <glib/gi18n-lib.h>

struct _FantasticObserver
{
  OttieRenderObserver parent;

  GskRenderNode *node;
  GHashTable *node_to_object;
  GSList *objects;
};

struct _FantasticObserverClass
{
  OttieRenderObserverClass parent_class;
};

G_DEFINE_TYPE (FantasticObserver, fantastic_observer, OTTIE_TYPE_RENDER_OBSERVER)

static void
fantastic_observer_start (OttieRenderObserver *observer,
                          OttieRender         *render,
                          double               timestamp)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (observer);

  g_clear_pointer (&self->node, gsk_render_node_unref);
  g_hash_table_remove_all (self->node_to_object);
}

static void
fantastic_observer_end (OttieRenderObserver *observer,
                        OttieRender         *render,
                        GskRenderNode       *node)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (observer);

  g_assert (self->objects == NULL);

  self->node = gsk_render_node_ref (node);
}

static void
fantastic_observer_start_object (OttieRenderObserver *observer,
                                 OttieRender         *render,
                                 OttieObject         *object,
                                 double               timestamp)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (observer);

  self->objects = g_slist_prepend (self->objects, object);
}

static void
fantastic_observer_end_object (OttieRenderObserver *observer,
                               OttieRender         *render,
                               OttieObject         *object)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (observer);

  self->objects = g_slist_remove (self->objects, object);
}

static void
fantastic_observer_add_node (OttieRenderObserver *observer,
                             OttieRender         *render,
                             GskRenderNode       *node)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (observer);

  g_hash_table_insert (self->node_to_object, 
                       gsk_render_node_ref (node),
                       g_object_ref (self->objects->data));
}

static void
fantastic_observer_finalize (GObject *object)
{
  FantasticObserver *self = FANTASTIC_OBSERVER (object);

  g_clear_pointer (&self->node, gsk_render_node_unref);
  g_hash_table_unref (self->node_to_object);

  G_OBJECT_CLASS (fantastic_observer_parent_class)->finalize (object);
}

static void
fantastic_observer_class_init (FantasticObserverClass *klass)
{
  OttieRenderObserverClass *observer_class = OTTIE_RENDER_OBSERVER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = fantastic_observer_finalize;

  observer_class->start = fantastic_observer_start;
  observer_class->end = fantastic_observer_end;
  observer_class->start_object = fantastic_observer_start_object;
  observer_class->end_object = fantastic_observer_end_object;
  observer_class->add_node = fantastic_observer_add_node;
}

static void
fantastic_observer_init (FantasticObserver *self)
{
  self->node_to_object = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                                (GDestroyNotify) gsk_render_node_unref,
                                                g_object_unref);
}

FantasticObserver *
fantastic_observer_new (void)
{
  return g_object_new (FANTASTIC_TYPE_OBSERVER, NULL);
}

static gboolean
transform_point_inverse (GskTransform           *self,
                         const graphene_point_t *point,
                         graphene_point_t       *out_point)
{
  switch (gsk_transform_get_category (self))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      *out_point = *point;
      return TRUE;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      {
        float dx, dy;

        gsk_transform_to_translate (self, &dx, &dy);
        out_point->x = point->x - dx;
        out_point->y = point->y - dy;
        return TRUE;
      }

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        float dx, dy, scale_x, scale_y;

        gsk_transform_to_affine (self, &scale_x, &scale_y, &dx, &dy);

        if (scale_x == 0 || scale_y == 0)
          return FALSE;

        out_point->x = (point->x - dx) / scale_x;
        out_point->y = (point->y - dy) / scale_y;
        return TRUE;
      }

    case GSK_TRANSFORM_CATEGORY_2D:
      {
        float dx, dy, xx, xy, yx, yy;
        float det, x, y;

        gsk_transform_to_2d (self, &xx, &yx, &xy, &yy, &dx, &dy);
        det = xx * yy - yx * xy;
        if (det == 0)
          return FALSE;

        x = point->x - dx;
        y = point->y - dy;
        out_point->x = (x * xx - y * xy) / det;
        out_point->y = (y * yy - x * yx) / det;
        return TRUE;
      }

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    default:
      g_warning ("FIXME: add 3D support");
      return FALSE;
    }
}

static GskRenderNode *
render_node_pick (FantasticObserver       *self,
                  GskRenderNode           *node,
                  const graphene_point_t  *p,
                  OttieObject            **out_object)
{
  GskRenderNode *result = NULL;
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (node, &bounds);
  if (!graphene_rect_contains_point (&bounds, p))
    return NULL;

  switch (gsk_render_node_get_node_type (node))
  {
    case GSK_CONTAINER_NODE:
      for (gsize i = gsk_container_node_get_n_children (node); i-- > 0; )
        {
          result = render_node_pick (self, gsk_container_node_get_child (node, i), p, out_object);
          if (result)
            break;
        }
      break;

    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_TEXTURE_NODE:
      result = node;
      break;

    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      g_assert_not_reached ();
      break;

    case GSK_TRANSFORM_NODE:
      {
        graphene_point_t tp;
        if (transform_point_inverse (gsk_transform_node_get_transform (node), p, &tp))
          result = render_node_pick (self, gsk_transform_node_get_child (node), &tp, out_object);
      }
      break;

    case GSK_OPACITY_NODE:
      result = render_node_pick (self, gsk_opacity_node_get_child (node), p, out_object);
      break;

    case GSK_COLOR_MATRIX_NODE:
      result = render_node_pick (self, gsk_color_matrix_node_get_child (node), p, out_object);
      break;

    case GSK_REPEAT_NODE:
      {
        GskRenderNode *child = gsk_repeat_node_get_child (node);
        graphene_point_t tp;
        
        gsk_render_node_get_bounds (child, &bounds);
        tp.x = p->x - bounds.origin.x;
        tp.y = p->y - bounds.origin.y;
        tp.x = fmod (tp.x, bounds.size.width);
        if (tp.x < 0)
          tp.x += bounds.size.width;
        tp.y = fmod (tp.y, bounds.size.height);
        if (tp.y < 0)
          tp.y += bounds.size.height;
        
        return render_node_pick (self, child, &tp, out_object);
      }

    case GSK_CLIP_NODE:
      result = render_node_pick (self, gsk_clip_node_get_child (node), p, out_object);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      if (gsk_rounded_rect_contains_point (gsk_rounded_clip_node_get_clip (node), p))
        result = render_node_pick (self, gsk_rounded_clip_node_get_child (node), p, out_object);
      break;

    case GSK_FILL_NODE:
      {
        GskPathMeasure *measure = gsk_path_measure_new (gsk_fill_node_get_path (node));
        gboolean in = gsk_path_measure_in_fill (measure, p, gsk_fill_node_get_fill_rule (node));
        gsk_path_measure_unref (measure);
        if (in)
          result = render_node_pick (self, gsk_fill_node_get_child (node), p, out_object);
      }
      break;
          
    case GSK_STROKE_NODE:
      {
        GskPathMeasure *measure = gsk_path_measure_new (gsk_stroke_node_get_path (node));
        float line_width = gsk_stroke_get_line_width (gsk_stroke_node_get_stroke (node));
        gboolean in = gsk_path_measure_get_closest_point_full (measure, p, line_width, NULL, NULL, NULL, NULL);
        gsk_path_measure_unref (measure);
        if (in)
          result = render_node_pick (self, gsk_stroke_node_get_child (node), p, out_object);
      }
      break;

    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_TEXT_NODE:
      g_assert_not_reached ();
      break;

    case GSK_BLUR_NODE:
      result = render_node_pick (self, gsk_blur_node_get_child (node), p, out_object);
      break;

    case GSK_DEBUG_NODE:
      result = render_node_pick (self, gsk_debug_node_get_child (node), p, out_object);
      break;

    case GSK_GL_SHADER_NODE:
      g_assert_not_reached ();
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      break;
  }

  if (result && out_object && *out_object == NULL)
    *out_object = g_hash_table_lookup (self->node_to_object, node);

  return result;
}

GskRenderNode *
fantastic_observer_pick_node (FantasticObserver *self,
                              double             x,
                              double             y)
{
  g_return_val_if_fail (FANTASTIC_IS_OBSERVER (self), NULL);

  if (self->node == NULL)
    return NULL;

  return render_node_pick (self, self->node, &GRAPHENE_POINT_INIT (x, y), NULL);
}

OttieObject *
fantastic_observer_pick (FantasticObserver *self,
                         double             x,
                         double             y)
{
  OttieObject *result = NULL;

  g_return_val_if_fail (FANTASTIC_IS_OBSERVER (self), NULL);

  if (self->node == NULL)
    return NULL;

  if (!render_node_pick (self, self->node, &GRAPHENE_POINT_INIT (x, y), &result))
    return NULL;

  return result;
}
