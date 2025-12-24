/* GSK - The GTK Scene Kit
 *
 * Copyright 2025  Benjamin Otte
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

#include "gskcopypasteutilsprivate.h"

#include "gskclipnode.h"
#include "gskcolornode.h"
#include "gskcontainernode.h"
#include "gskcopynode.h"
#include "gskisolationnode.h"
#include "gskopacitynode.h"
#include "gskpastenode.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"
#include "gsktransform.h"
#include "gsktransformnode.h"

#include "gdk/gdkrgbaprivate.h"

typedef struct _PartialNode PartialNode;
struct _PartialNode
{
  GskRenderNodeType node_type;
  union {
    GPtrArray *container;
    GskTransform *transform;
  };
  const PartialNode *next;
};

typedef struct _Copy Copy;
struct _Copy {
  const PartialNode *nodes_copied;
  const Copy *next_copy;
};

typedef struct {
  const PartialNode *nodes;
  const Copy *copies;
} Recording;

static GskRenderNode *
replay_partial_node (const PartialNode *replay)
{
  GskRenderNode *node = NULL;
  GskTransform *transform = NULL;

  for (; replay != NULL; replay = replay->next)
    {
      switch (replay->node_type)
        {
        case GSK_TRANSFORM_NODE:
          {
            GskTransform *tmp;
            GskRenderNode *tmp_node;
            if (node)
              {
                tmp_node = gsk_transform_node_new (node, replay->transform);
                gsk_render_node_unref (node);
                node = tmp_node;
              }
            tmp = gsk_transform_transform (gsk_transform_ref (replay->transform), transform);
            gsk_transform_unref (transform);
            transform = tmp;
          }
          break;

        case GSK_CONTAINER_NODE:
          if (node)
            {
              g_ptr_array_add (replay->container, node);
              node = gsk_container_node_new ((GskRenderNode **) replay->container->pdata, replay->container->len);
              g_ptr_array_set_size (replay->container, replay->container->len - 1);
            }
          else
            {
              node = gsk_container_node_new ((GskRenderNode **) replay->container->pdata, replay->container->len);
            }
          break;

        case GSK_CAIRO_NODE:
        case GSK_COLOR_NODE:
        case GSK_LINEAR_GRADIENT_NODE:
        case GSK_REPEATING_LINEAR_GRADIENT_NODE:
        case GSK_RADIAL_GRADIENT_NODE:
        case GSK_REPEATING_RADIAL_GRADIENT_NODE:
        case GSK_CONIC_GRADIENT_NODE:
        case GSK_BORDER_NODE:
        case GSK_TEXTURE_NODE:
        case GSK_INSET_SHADOW_NODE:
        case GSK_OUTSET_SHADOW_NODE:
        case GSK_COMPONENT_TRANSFER_NODE:
        case GSK_TEXT_NODE:
        case GSK_TEXTURE_SCALE_NODE:
        case GSK_OPACITY_NODE:
        case GSK_COLOR_MATRIX_NODE:
        case GSK_REPEAT_NODE:
        case GSK_SHADOW_NODE:
        case GSK_BLEND_NODE:
        case GSK_CROSS_FADE_NODE:
        case GSK_BLUR_NODE:
        case GSK_GL_SHADER_NODE:
        case GSK_MASK_NODE:
        case GSK_DEBUG_NODE:
        case GSK_CLIP_NODE:
        case GSK_ROUNDED_CLIP_NODE:
        case GSK_FILL_NODE:
        case GSK_STROKE_NODE:
        case GSK_SUBSURFACE_NODE:
        case GSK_COPY_NODE:
        case GSK_PASTE_NODE:
        case GSK_COMPOSITE_NODE:
        case GSK_ISOLATION_NODE:
        case GSK_DISPLACEMENT_NODE:
          /* These all don't record anything, so we never
           * encounter them */
        case GSK_NOT_A_RENDER_NODE:
        default:
          g_assert_not_reached ();
        }
    }

  if (node && transform)
    {
      transform = gsk_transform_invert (transform);
      if (transform)
        {
          GskRenderNode *tmp;

          if (node)
            {
              tmp = gsk_transform_node_new (node, transform);
              gsk_render_node_unref (node);
              node = tmp;
            }
        }
      else
        {
          g_warning ("Trying to paste non-invertible transform, ignoring.");
        }
    }
  g_clear_pointer (&transform, gsk_transform_unref);
  if (node)
    {
      GskRenderNode *tmp = gsk_isolation_node_new (node, GSK_ISOLATION_ALL);
      gsk_render_node_unref (node);
      node = tmp;
    }

  return node;
}

static GskRenderNode *
replace_copy_paste_node_record (GskRenderReplay *replay,
                                GskRenderNode   *node,
                                gpointer         user_data)
{
  Recording *recording = user_data;
  GskRenderNode *result;

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_TEXT_NODE:
    case GSK_DEBUG_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_SUBSURFACE_NODE:
      /* keep recording */
      result = gsk_render_replay_default (replay, node);
      break;

    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_BLUR_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_MASK_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_DISPLACEMENT_NODE:
      /* record a new background for each child */
      {
        const PartialNode *saved = recording->nodes;
        recording->nodes = NULL;
        result = gsk_render_replay_default (replay, node);
        recording->nodes = saved;
      }
      break;

    case GSK_ISOLATION_NODE:
      /* Do either of the 2 above, depending on flags */
      {
        GskIsolation isolations = gsk_isolation_node_get_isolations (node);
        Recording saved = *recording;
        if (isolations & GSK_ISOLATION_BACKGROUND)
          recording->nodes = NULL;
        if (isolations & GSK_ISOLATION_COPY_PASTE)
          recording->copies = NULL;
        result = gsk_render_replay_default (replay, node);
        *recording = saved;
      }
      break;

    case GSK_TRANSFORM_NODE:
      /* store the transform so we can play it back later */
      {
        PartialNode partial = {
          .node_type = GSK_TRANSFORM_NODE,
          .transform = gsk_transform_node_get_transform (node),
          .next = recording->nodes,
        };
        recording->nodes = &partial;
        result = gsk_render_replay_default (replay, node);
        recording->nodes = partial.next;
      }
      break;

    case GSK_CONTAINER_NODE:
      /* replay things node by node to allow copy nodes inside */
      {
        PartialNode partial = {
          .node_type = GSK_CONTAINER_NODE,
          .container = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref),
          .next = recording->nodes,
        };
        gboolean changed;
        guint i;

        recording->nodes = &partial;
        changed = FALSE;

        for (i = 0;
             i < gsk_container_node_get_n_children (node);
             i++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);
            GskRenderNode *replayed = gsk_render_replay_filter_node (replay, child);

            if (replayed != child)
              changed = TRUE;

            if (replayed != NULL)
              g_ptr_array_add (partial.container, replayed);
          }

        if (!changed)
          result = gsk_render_node_ref (node);
        else
          result = gsk_container_node_new ((GskRenderNode **) partial.container->pdata,
                                           partial.container->len);

        g_ptr_array_unref (partial.container);
        recording->nodes = partial.next;
      }
      break;

    case GSK_COPY_NODE:
      /* keep recording on the current background, but also snapshot the
       * current state for replay */
      {
        Copy copy = {
          .nodes_copied = recording->nodes,
          .next_copy = recording->copies,
        };
        /* skip this node */
        recording->copies = &copy;
        result = gsk_render_replay_filter_node (replay, gsk_copy_node_get_child (node));
        recording->copies = copy.next_copy;
      }
      break;

    case GSK_PASTE_NODE:
      {
        const Copy *paste = recording->copies;
        gsize i;

        for (i = 0; i < gsk_paste_node_get_depth (node); i++)
          {
            if (paste == NULL)
              break;

            paste = paste->next_copy;
          }
  
        if (paste)
          {
            GskRenderNode *child = replay_partial_node (paste->nodes_copied);
            if (child)
              {
                result = gsk_clip_node_new (child, &node->bounds);
                gsk_render_node_unref (child);
              }
            else
              result = gsk_container_node_new (NULL, 0);
          }
        else
          result = gsk_container_node_new (NULL, 0);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
    }

  return result;
}

GskRenderNode *
gsk_render_node_replace_copy_paste (GskRenderNode *node)
{
  Recording recording = { NULL, NULL };
  GskRenderReplay *replay;
  GskRenderNode *result;
  
  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     replace_copy_paste_node_record,
                                     &recording,
                                     NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  if (result == NULL)
    result = gsk_color_node_new (&GDK_RGBA_TRANSPARENT, &node->bounds);

  gsk_render_node_unref (node);

  return result;
}

