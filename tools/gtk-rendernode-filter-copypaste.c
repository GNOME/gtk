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
#include "gtk-rendernode-tool.h"

/* nothing here is evil, so you can just skip to the comment that starts the evil */
#include "gsk/gskcopypasteutilsprivate.h"

static gboolean
gsk_render_node_isolates_background (const GskRenderNode *node)
{
  /* This should return the same thing as node->isolates_background but if not: oh well */

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_BLEND_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_BLUR_NODE:
    case GSK_OPACITY_NODE:
    case GSK_REPEAT_NODE:
    case GSK_SHADOW_NODE:
    case GSK_MASK_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_DISPLACEMENT_NODE:
    case GSK_ARITHMETIC_NODE:
      return TRUE;

    case GSK_ISOLATION_NODE:
      return gsk_isolation_node_get_isolations (node) & GSK_ISOLATION_BACKGROUND ? TRUE : FALSE;

    case GSK_CONTAINER_NODE:
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
    case GSK_TRANSFORM_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_TEXT_NODE:
    case GSK_DEBUG_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COPY_NODE:
    case GSK_PASTE_NODE:
      return FALSE;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

#include "gsk/gskcopypasteutils.c"

/* this comment starts the evil */

GskRenderNode *
filter_copypaste (GskRenderNode  *node,
                  int             argc,
                  const char    **argv)
{
  const GOptionEntry entries[] = {
    { NULL, }
  };
  GOptionContext *context;
  GError *error = NULL;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Replace copy/paste nodes with copies of nodes"));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      g_printerr ("copypaste: %s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  if (argc != 1)
    {
      g_printerr ("copypaste: Unexpected arguments\n");
      exit (1);
    }

  return gsk_render_node_replace_copy_paste (node);
}
