/*  Copyright 2026 Benjamin Otte
 *
 * GTK is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GTK is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK; see the file COPYING.  If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include "rntcontext.h"
#include "rntfilter.h"

#include "gsk/gskrectprivate.h"
#include "gsk/gskrendernodeprivate.h"
#include "gsk/gsktransformprivate.h"

static GskRenderNode *
cull_transform_node (GskRenderReplay  *replay,
                     GskRenderNode    *node,
                     graphene_rect_t **clip)
{
  GskTransform *transform;
  graphene_rect_t transformed_clip;
  graphene_rect_t *save;
  GskRenderNode *result;

  transform = gsk_transform_node_get_transform (node);
  save = *clip;

  if (*clip == NULL || gsk_transform_get_fine_category (transform) < GSK_FINE_TRANSFORM_CATEGORY_2D_DIHEDRAL)
    {
      *clip = NULL;
    }
  else
    {
      gsk_transform_transform_bounds (transform, *clip, &transformed_clip);
      *clip = &transformed_clip;
    }

  result = gsk_render_replay_default (replay, node);
  *clip = save;

  return result;
}

static GskRenderNode *
cull_clip_node (GskRenderReplay  *replay,
                GskRenderNode    *node,
                graphene_rect_t **clip)
{
  graphene_rect_t clipped_clip;
  graphene_rect_t *save;
  GskRenderNode *result;

  save = *clip;

  if (*clip)
    {
      if (!gsk_rect_intersection (gsk_clip_node_get_clip (node),
                                  *clip,
                                  &clipped_clip))
        return NULL;
    }
  else
    {
      clipped_clip = *gsk_clip_node_get_clip (node);
    }

  *clip = &clipped_clip;
  result = gsk_render_replay_default (replay, node);
  *clip = save;

  return result;
}

static GskRenderNode *
cull_rounded_clip_node (GskRenderReplay  *replay,
                        GskRenderNode    *node,
                        graphene_rect_t **clip)
{
  graphene_rect_t clipped_clip;
  graphene_rect_t *save;
  GskRenderNode *result;

  save = *clip;

  if (*clip)
    {
      if (!gsk_rect_intersection (&gsk_rounded_clip_node_get_clip (node)->bounds,
                                  *clip,
                                  &clipped_clip))
        return NULL;
    }
  else
    {
      clipped_clip = gsk_rounded_clip_node_get_clip (node)->bounds;
    }

  *clip = &clipped_clip;
  result = gsk_render_replay_default (replay, node);
  *clip = save;

  return result;
}

static GskRenderNode *
cull_node (GskRenderReplay *replay,
           GskRenderNode   *node,
           gpointer         user_data)
{
  graphene_rect_t **clip = (graphene_rect_t **) user_data;

  if (*clip && !gsk_rect_intersects (*clip, &node->bounds))
    return NULL;

  switch (gsk_render_node_get_node_type (node))
  {
    case GSK_TRANSFORM_NODE:
      return cull_transform_node (replay, node, clip);
      
    case GSK_CLIP_NODE:
      return cull_clip_node (replay, node, clip);

    case GSK_ROUNDED_CLIP_NODE:
      return cull_rounded_clip_node (replay, node, clip);

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
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_PASTE_NODE:
    case GSK_TURBULENCE_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_DEBUG_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COPY_NODE:
    case GSK_CONTAINER_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLUR_NODE:
    case GSK_DISPLACEMENT_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_MASK_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_ARITHMETIC_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
      return gsk_render_replay_default (replay, node);

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return gsk_render_replay_default (replay, node);
  }
}

GskRenderNode *
filter_cull (GskRenderNode  *node,
              RntContext     *ctx,
              int             argc,
              const char    **argv)
{
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GskRenderReplay *replay;
  GskRenderNode *result;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Cull nodes that are provably clipped"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      rnt_context_error (ctx, "cull: %s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  if (argc != 1)
    {
      rnt_context_error (ctx, "Unexpected arguments\n");
      return NULL;
    }

  replay = gsk_render_replay_new ();
  gsk_render_replay_set_node_filter (replay,
                                     cull_node,
                                     (graphene_rect_t *[1]) { NULL, },
                                     NULL);
  result = gsk_render_replay_filter_node (replay, node);
  gsk_render_replay_free (replay);

  return result;
}
