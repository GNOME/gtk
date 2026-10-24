/*  Copyright 2026 Red Hat, Inc.
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
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include "gtk-rendernode-tool.h"
#include "gtk-tool-utils.h"
#include "rntcontext.h"

#define N_NODE_TYPES (GSK_ARITHMETIC_NODE + 1)

static int child_count[N_NODE_TYPES] = {
  [GSK_NOT_A_RENDER_NODE] = 0,
  [GSK_CONTAINER_NODE] = -1,
  [GSK_CAIRO_NODE] = 0,
  [GSK_COLOR_NODE] = 0,
  [GSK_LINEAR_GRADIENT_NODE] = 0,
  [GSK_REPEATING_LINEAR_GRADIENT_NODE] = 0,
  [GSK_RADIAL_GRADIENT_NODE] = 0,
  [GSK_REPEATING_RADIAL_GRADIENT_NODE] = 0,
  [GSK_CONIC_GRADIENT_NODE] = 0,
  [GSK_BORDER_NODE] = 0,
  [GSK_TEXTURE_NODE] = 0,
  [GSK_INSET_SHADOW_NODE] = 0,
  [GSK_OUTSET_SHADOW_NODE] = 0,
  [GSK_TRANSFORM_NODE] = 1,
  [GSK_OPACITY_NODE] = 1,
  [GSK_COLOR_MATRIX_NODE] = 1,
  [GSK_REPEAT_NODE] = 1,
  [GSK_CLIP_NODE] = 1,
  [GSK_ROUNDED_CLIP_NODE] = 1,
  [GSK_SHADOW_NODE] = 1,
  [GSK_BLEND_NODE] = 2,
  [GSK_CROSS_FADE_NODE] = 2,
  [GSK_TEXT_NODE] = 0,
  [GSK_BLUR_NODE] = 1,
  [GSK_DEBUG_NODE] = 1,
  [GSK_TEXTURE_SCALE_NODE] = 0,
  [GSK_MASK_NODE] = 2,
  [GSK_STROKE_NODE] = 1,
  [GSK_FILL_NODE] = 1,
  [GSK_SUBSURFACE_NODE] = 1,
  [GSK_COMPONENT_TRANSFER_NODE] = 1,
  [GSK_COPY_NODE] = 1,
  [GSK_PASTE_NODE] = 0,
  [GSK_COMPOSITE_NODE] = 2,
  [GSK_ISOLATION_NODE] = 1,
  [GSK_DISPLACEMENT_NODE] = 2,
  [GSK_ARITHMETIC_NODE] = 2,
};

#define ANY_NODE 0xffff

typedef struct _NodePattern NodePattern;
struct _NodePattern
{
  GskRenderNodeType type;
  size_t count;
  NodePattern **children;
};

static void
node_pattern_free (NodePattern *pattern)
{
  if (pattern == NULL)
    return;

  for (size_t i = 0; i < pattern->count; i++)
    node_pattern_free (pattern->children[i]);
  g_free (pattern->children);
  g_free (pattern);
}

static GskRenderNodeType
find_render_node_type (const char *s,
                       size_t      len)
{
  for (unsigned int i = 1; i < N_NODE_TYPES; i++)
    {
      if (strncmp (s, get_node_name (i), len) == 0)
        return i;
    }

  return 0;
}

static NodePattern *
node_pattern_parse (const char **string)
{
  NodePattern *pattern;

  pattern = g_new0 (NodePattern, 1);

  if ((*string)[0] == '(')
    {
      const char *p;

      (*string)++;

      p = strchr (*string, ' ');
      if (!p)
        p = *string + strlen (*string);
      pattern->type = find_render_node_type (*string, p - *string);

      *string = p + 1;

      if (pattern->type == 0)
        {
          g_clear_pointer (&pattern, node_pattern_free);
        }
      else
        {
          if (pattern->type == GSK_CONTAINER_NODE)
            {
              p = strchr (*string, ' ');
              pattern->count = atoi (*string);
              *string = p + 1;
            }
          else
            pattern->count = child_count[pattern->type];

          pattern->children = g_new0 (NodePattern *, pattern->count);

          for (size_t i = 0; i < pattern->count; i++)
            {
              pattern->children[i] = node_pattern_parse (string);
              if (pattern->children[i] == NULL)
                {
                  node_pattern_free (pattern);
                  return NULL;
                }

              if (i + 1 < pattern->count)
                {
                  if ((*string)[0] != ' ')
                    {
                      node_pattern_free (pattern);
                      return NULL;
                    }

                  (*string)++;
                }
            }
        }

      if ((*string)[0] != ')')
        {
          node_pattern_free (pattern);
          return NULL;
        }
    }
  else if ((*string)[0] == '.')
    {
      pattern->type = ANY_NODE;
      pattern->count = 0;
      (*string)++;
    }
  else
    {
      const char *p;

      p = strpbrk (*string, " )");
      if (!p)
        p = *string + strlen (*string);

      pattern->type = find_render_node_type (*string, p - *string);
      if (pattern->type == 0)
        {
          node_pattern_free (pattern);
          return NULL;
        }
      pattern->count = 0;

      *string += strlen (get_node_name (pattern->type));
    }

  return pattern;
}

static gboolean
node_pattern_matches (NodePattern   *pattern,
                      GskRenderNode *node)
{
  GskRenderNode **children;
  size_t count;

  if (pattern->type == ANY_NODE)
    return TRUE;

  if (gsk_render_node_get_node_type (node) != pattern->type)
    return FALSE;

  if (pattern->count == 0)
    return TRUE;

  children = gsk_render_node_get_children (node, &count);
  if (count != pattern->count)
    return FALSE;

  for (size_t i = 0; i < count; i++)
    {
      if (!node_pattern_matches (pattern->children[i], children[i]))
        return FALSE;
    }

  return TRUE;
}

static void
node_pattern_find_matches (NodePattern   *pattern,
                           GskRenderNode *node,
                           GPtrArray     *matches)
{
  GskRenderNode **children;
  size_t n_children;

  if (node_pattern_matches (pattern, node))
    g_ptr_array_add (matches, gsk_render_node_ref (node));

  children = gsk_render_node_get_children (node, &n_children);

  for (size_t i = 0; i < n_children; i++)
    node_pattern_find_matches (pattern, children[i], matches);
}

static GPtrArray *
find_matches (GskRenderNode *node,
              const char    *string)
{
  NodePattern *pattern;
  GPtrArray *matches;

  pattern = node_pattern_parse (&string);
  if (pattern == NULL)
    {
      g_printerr (_("Failed to parse node pattern\n"));
      exit (1);
    }

  matches = g_ptr_array_new ();
  g_ptr_array_set_free_func (matches, (GDestroyNotify) gsk_render_node_unref);

  node_pattern_find_matches (pattern, node, matches);

  node_pattern_free (pattern);

  return matches;
}

GskRenderNode *
filter_match (GskRenderNode  *node,
              RntContext     *ctx,
              int             argc,
              const char    **argv)
{
  GOptionContext *context;
  char **patterns = NULL;
  gboolean count = FALSE;
  gboolean keep = FALSE;
  const GOptionEntry entries[] = {
    { "count", 0, 0, G_OPTION_ARG_NONE, &count, N_("Count the matches instead"), NULL },
    { "keep", 0, 0, G_OPTION_ARG_NONE, &keep, N_("Keep existing nodes in stack"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &patterns, NULL, N_("PATTERN") },
    { NULL, }
  };
  GError *error = NULL;
  GPtrArray *matches;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Match patterns in the render node."));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (patterns == NULL || g_strv_length (patterns) != 1)
    {
      g_printerr (_("Can only accept a pattern\n"));
      exit (1);
    }

  gsk_render_node_ref (node);

  matches = find_matches (node, patterns[0]);
      
  if (count)
    {
      rnt_context_print (ctx, "found %u matches\n", matches->len);
    }
  else 
    {
      if (keep)
        {
          rnt_context_push_node (ctx, node);
        }
      else
        {
          rnt_context_clear_nodes (ctx);
          gsk_render_node_unref (node);
        }

      while (matches->len > 1)
        {
          rnt_context_push_node (ctx, g_ptr_array_steal_index (matches, matches->len - 1));
        }

      if (matches->len > 0)
        {
          node = g_ptr_array_steal_index (matches, 0);
        }
      else
        {
          rnt_context_set_failed (ctx, TRUE);
          if (keep)
            node = rnt_context_pop_node (ctx);
          else
            node = gsk_render_node_ref (rnt_context_get_null_node (ctx));
        }
    }

  g_ptr_array_free (matches, TRUE);

  g_strfreev (patterns);

  return node;
}
