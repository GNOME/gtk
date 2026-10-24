/*  Copyright 2023 Red Hat, Inc.
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
#ifdef CAIRO_HAS_SVG_SURFACE
#include <cairo-svg.h>
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
#include <cairo-pdf.h>
#endif
#include "rntcontext.h"

#if defined(CAIRO_HAS_SVG_SURFACE) || defined(CAIRO_HAS_PDF_SURFACE)
static cairo_status_t
cairo_serializer_write (gpointer             user_data,
                        const unsigned char *data,
                        unsigned int         length)
{
  g_byte_array_append (user_data, data, length);

  return CAIRO_STATUS_SUCCESS;
}
#endif

#ifdef CAIRO_HAS_SVG_SURFACE
static GBytes *
create_svg (GskRenderNode  *node,
            GError        **error)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_svg_surface_create_for_stream (cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_svg_surface_set_document_unit (surface, CAIRO_SVG_UNIT_PX);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return g_byte_array_free_to_bytes (array);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", cairo_status_to_string (cairo_surface_status (surface)));
      cairo_surface_destroy (surface);
      g_byte_array_unref (array);
      return NULL;
    }
}
#endif

#ifdef CAIRO_HAS_PDF_SURFACE
static GBytes *
create_pdf (GskRenderNode  *node,
            GError        **error)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  graphene_rect_t bounds;
  GByteArray *array;

  gsk_render_node_get_bounds (node, &bounds);
  array = g_byte_array_new ();

  surface = cairo_pdf_surface_create_for_stream (cairo_serializer_write,
                                                 array,
                                                 bounds.size.width,
                                                 bounds.size.height);
  cairo_surface_set_device_offset (surface, -bounds.origin.x, -bounds.origin.y);

  cr = cairo_create (surface);
  gsk_render_node_draw (node, cr);
  cairo_show_page (cr);
  cairo_destroy (cr);

  cairo_surface_finish (surface);
  if (cairo_surface_status (surface) == CAIRO_STATUS_SUCCESS)
    {
      cairo_surface_destroy (surface);
      return g_byte_array_free_to_bytes (array);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "%s", cairo_status_to_string (cairo_surface_status (surface)));
      cairo_surface_destroy (surface);
      g_byte_array_unref (array);
      return NULL;
    }
}
#endif

static gboolean
render_node (GskRenderNode         *node,
             RntContext            *ctx,
             const char            *renderer_name,
             const graphene_rect_t *bounds,
             gboolean               snap,
             const char            *save_file)
{
  GBytes *bytes;
  GError *error = NULL;

#ifdef CAIRO_HAS_SVG_SURFACE
  if (g_str_has_suffix (save_file, ".svg"))
    {
      bytes = create_svg (node, &error);
      if (bytes == NULL)
        {
          rnt_context_error (ctx, _("Failed to generate SVG: %s\n"), error->message);
          g_clear_error (&error);
          return FALSE;
        }
    }
  else
#endif
#ifdef CAIRO_HAS_PDF_SURFACE
  if (g_str_has_suffix (save_file, ".pdf"))
    {
      bytes = create_pdf (node, &error);
      if (bytes == NULL)
        {
          rnt_context_error (ctx, _("Failed to generate SVG: %s\n"), error->message);
          g_clear_error (&error);
          return FALSE;
        }
    }
  else
#endif
    {
      GdkTexture *texture;
      GskRenderer *renderer;
      graphene_rect_t node_bounds;

      renderer = create_renderer (renderer_name, &error);
      if (renderer == NULL)
        {
          rnt_context_error (ctx, _("Failed to create renderer: %s\n"), error->message);
          g_clear_error (&error);
          return FALSE;
        }

      if (bounds == NULL)
        {
          gsk_render_node_get_bounds (node, &node_bounds);
          if (snap)
            {
              graphene_rect_t snapped;

              snapped.origin.x = floorf (node_bounds.origin.x);
              snapped.origin.y = floorf (node_bounds.origin.y);
              snapped.size.width = ceilf (node_bounds.origin.x + node_bounds.size.width) - snapped.origin.x;
              snapped.size.height = ceilf (node_bounds.origin.y + node_bounds.size.height) - snapped.origin.y;

              node_bounds = snapped;
            }
          bounds = &node_bounds;
        }

      texture = gsk_renderer_render_texture (renderer, node, bounds);

      if (g_str_has_suffix (save_file, ".tif") ||
          g_str_has_suffix (save_file, ".tiff"))
        bytes = gdk_texture_save_to_tiff_bytes (texture);
      else
        bytes = gdk_texture_save_to_png_bytes (texture);

      g_object_unref (texture);
    }

  if (g_file_set_contents (save_file,
                           g_bytes_get_data (bytes, NULL),
                           g_bytes_get_size (bytes),
                           &error))
    {
      rnt_context_verbose (ctx, _("Output written to %s.\n"), save_file);
    }
  else
    {
      rnt_context_error (ctx, _("Failed to save %s: %s\n"), save_file, error->message);
      g_clear_error (&error);
      return FALSE;
    }

  g_bytes_unref (bytes);

  return TRUE;
}

static gboolean
my_strtof (const char *str,
           float      *result)
{
  char *end;

  if (str[0] == 0)
    return FALSE;

  *result = g_ascii_strtod (str, &end);
  return *end == 0;
}

static gboolean
rect_init_from_arg (graphene_rect_t *rect,
                    const char      *arg)
{
  char **split = g_strsplit (arg, ",", 4);
  gboolean result;

  result = g_strv_length (split) == 4 &&
           my_strtof(split[0], &rect->origin.x) &&
           my_strtof(split[1], &rect->origin.y) &&
           my_strtof(split[2], &rect->size.width) &&
           my_strtof(split[3], &rect->size.height);

  g_strfreev (split);
  return result;
}

GskRenderNode *
filter_render (GskRenderNode  *node,
               RntContext     *ctx,
               int             argc,
               const char    **argv)
{
  GOptionContext *context;
  char **filenames = NULL;
  char *renderer = NULL;
  char *bounds_str = NULL;
  graphene_rect_t bounds;
  gboolean snap = FALSE;
  const GOptionEntry entries[] = {
    { "renderer", 0, 0, G_OPTION_ARG_STRING, &renderer, N_("Renderer to use"), N_("RENDERER") },
    { "dont-move", 0, 0, G_OPTION_ARG_NONE, &snap, N_("Keep node position unchanged"),  NULL },
    { "bounds", 0, 0, G_OPTION_ARG_STRING, &bounds_str, N_("Area to render"), N_("X,Y,W,H") },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL, N_("FILE…") },
    { NULL, }
  };
  GskRenderNode *n;
  GError *error = NULL;
  gsize i;

  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Render to an image."));

  if (!g_option_context_parse (context, &argc, (char ***) &argv, &error))
    {
      rnt_context_error (ctx, "%s\n", error->message);
      g_error_free (error);
      return NULL;
    }

  g_option_context_free (context);

  if (filenames == NULL)
    {
      rnt_context_error (ctx, _("No filename specified\n"));
      return NULL;
    }

  if (bounds_str)
    {
      if (!rect_init_from_arg (&bounds, bounds_str))
        {
          rnt_context_error (ctx, _("Invalid rectangle given for bounds\n"));
          return NULL;
        }
    }

  for (i = 0, n = node; filenames[i]; i++)
    {
      if (n == NULL)
        {
          rnt_context_error (ctx, _("%u filenames given but only %zu nodes available\n"),
                             g_strv_length (filenames), i);
          return NULL;
        }

      if (!render_node (n,
                        ctx,
                        renderer,
                        bounds_str ? &bounds : NULL,
                        snap,
                        filenames[i]))
        return NULL;

      n = rnt_context_get_node (ctx, i);
    }

  g_strfreev (filenames);

  return gsk_render_node_ref (node);
}
