/*
 * Copyright (C) 2017 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include "reftest-compare.h"

static void
hsv_to_rgb (GdkRGBA *rgba,
            gdouble  h,
            gdouble  s,
            gdouble  v)
{
  gdouble hue, saturation, value;
  gdouble f, p, q, t;

  rgba->alpha = 1.0;

  if ( s == 0.0)
    {
      rgba->red = v;
      rgba->green = v;
      rgba->blue = v; /* heh */
    }
  else
    {
      hue = h * 6.0;
      saturation = s;
      value = v;

      if (hue == 6.0)
        hue = 0.0;

      f = hue - (int) hue;
      p = value * (1.0 - saturation);
      q = value * (1.0 - saturation * f);
      t = value * (1.0 - saturation * (1.0 - f));

      switch ((int) hue)
        {
        case 0:
          rgba->red = value;
          rgba->green = t;
          rgba->blue = p;
          break;

        case 1:
          rgba->red = q;
          rgba->green = value;
          rgba->blue = p;
          break;

        case 2:
          rgba->red = p;
          rgba->green = value;
          rgba->blue = t;
          break;

        case 3:
          rgba->red = p;
          rgba->green = q;
          rgba->blue = value;
          break;

        case 4:
          rgba->red = t;
          rgba->green = p;
          rgba->blue = value;
          break;

        case 5:
          rgba->red = value;
          rgba->green = p;
          rgba->blue = q;
          break;

        default:
          g_assert_not_reached ();
        }
    }
}

static GskRenderNode *
colors (void)
{
  GskRenderNode **nodes = g_newa (GskRenderNode *, 1000);
  GskRenderNode *container;
  graphene_rect_t bounds;
  GdkRGBA color;
  guint i;

  for (i = 0; i < 1000; i++)
    {
      bounds.size.width = g_random_int_range (20, 100);
      bounds.origin.x = g_random_int_range (0, 1000 - bounds.size.width);
      bounds.size.height = g_random_int_range (20, 100);
      bounds.origin.y = g_random_int_range (0, 1000 - bounds.size.height);
      hsv_to_rgb (&color, g_random_double (), g_random_double_range (0.15, 0.4), g_random_double_range (0.6, 0.85));
      color.alpha = g_random_double_range (0.5, 0.75);
      nodes[i] = gsk_color_node_new (&color, &bounds);
    }

  container = gsk_container_node_new (nodes, 1000);

  for (i = 0; i < 1000; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

static GskRenderNode *
cairo (void)
{
  GskRenderNode *node;
  cairo_t *cr;

  node = gsk_cairo_node_new (&GRAPHENE_RECT_INIT (0, 0, 200, 600));
  cr = gsk_cairo_node_get_draw_context (node, NULL);

  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_rectangle (cr, 0, 0, 200, 200);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0, 1, 0);
  cairo_rectangle (cr, 0, 200, 200, 200);
  cairo_fill (cr);
  cairo_set_source_rgb (cr, 0, 0, 1);
  cairo_rectangle (cr, 0, 400, 200, 200);
  cairo_fill (cr);

  cairo_destroy (cr);

  return node;
}

static GskRenderNode *
cairo2 (void)
{
  GskRenderNode *node;
  cairo_t *cr;
  int i, j;

  node = gsk_cairo_node_new (&GRAPHENE_RECT_INIT (0, 0, 200, 200));
  cr = gsk_cairo_node_get_draw_context (node, NULL);

  cairo_set_source_rgb (cr, 1, 1, 1);

  for (i = 0; i < 10; i++)
    for (j = 0; j < 10; j++)
      {
        cairo_rectangle (cr, i*20, j*20, 10, 10);
        cairo_fill (cr);
      }


  cairo_destroy (cr);

  return node;
}

static GskRenderNode *
repeat (void)
{
  GskRenderNode *repeat[4];
  GskRenderNode *child;
  GskRenderNode *transform;
  GskRenderNode *container;
  graphene_matrix_t matrix;

  child = cairo ();

  repeat[0] = gsk_repeat_node_new (&GRAPHENE_RECT_INIT (0, 0, 200, 200),
                                   child,
                                   &GRAPHENE_RECT_INIT (0, 0, 200, 600));
  repeat[1] = gsk_repeat_node_new (&GRAPHENE_RECT_INIT (0, 200, 200, 200),
                                   child,
                                   &GRAPHENE_RECT_INIT (0, 0, 200, 600));
  repeat[2] = gsk_repeat_node_new (&GRAPHENE_RECT_INIT (0, 400, 200, 200),
                                   child,
                                   &GRAPHENE_RECT_INIT (0, 0, 200, 600));
  repeat[3] = gsk_repeat_node_new (&GRAPHENE_RECT_INIT (0, 100, 200, 640),
                                   child,
                                   &GRAPHENE_RECT_INIT (0, 100, 200, 400));

  gsk_render_node_unref (child);

  graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { 0, 20, 0 });
  transform = gsk_transform_node_new (repeat[1], &matrix);
  gsk_render_node_unref (repeat[1]);
  repeat[1] = transform;

  graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { 0, 40, 0 });
  transform = gsk_transform_node_new (repeat[2], &matrix);
  gsk_render_node_unref (repeat[2]);
  repeat[2] = transform;

  graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { 220, -100, 0 });
  transform = gsk_transform_node_new (repeat[3], &matrix);
  gsk_render_node_unref (repeat[3]);
  repeat[3] = transform;

  container = gsk_container_node_new (repeat, 4);

  gsk_render_node_unref (repeat[0]);
  gsk_render_node_unref (repeat[1]);
  gsk_render_node_unref (repeat[2]);
  gsk_render_node_unref (repeat[3]);

  return container;
}

static GskRenderNode *
blendmode (void)
{
  GskRenderNode *child1;
  GskRenderNode *child2;
  GskRenderNode *transform;
  GskRenderNode *container;
  graphene_matrix_t matrix;

  child1 = cairo ();
  child2 = cairo2 ();

  graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { 50, 50, 0 });
  transform = gsk_transform_node_new (child2, &matrix);
  gsk_render_node_unref (child2);
  child2 = transform;

  container = gsk_blend_node_new (child1, child2, GSK_BLEND_MODE_HUE);

  gsk_render_node_unref (child1);
  gsk_render_node_unref (child2);

  return container;
}

static GskRenderNode *
ducky (void)
{
  GdkPixbuf *pixbuf;
  GskRenderNode *node;
  cairo_t *cr;

  pixbuf = gdk_pixbuf_new_from_file_at_size ("ducky.png", 100, 100, NULL);
  node = gsk_cairo_node_new (&GRAPHENE_RECT_INIT (0, 0,
                                                  gdk_pixbuf_get_width (pixbuf),
                                                  gdk_pixbuf_get_height (pixbuf)));
  cr = gsk_cairo_node_get_draw_context (node, NULL);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
  g_object_unref (pixbuf);

  return node;
}

static GskRenderNode *
gradient (void)
{
  return gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (0, 0, 100, 100),
                                       &GRAPHENE_POINT_INIT (0, 0),
                                       &GRAPHENE_POINT_INIT (0, 100),
                                       (const GskColorStop[3]) {
                                         { .offset = 0.0, .color = { 1.0, 0.0, 0.0, 1.0 } },
                                         { .offset = 0.5, .color = { 0.0, 1.0, 0.0, 1.0 } },
                                         { .offset = 1.0, .color = { 0.0, 0.0, 1.0, 1.0 } }
                                       },
                                       3);
}

static GskRenderNode *
blendmodes (void)
{
  GskRenderNode *child1;
  GskRenderNode *child2;
  GskRenderNode *container;
  GskRenderNode *blend[16];
  GskBlendMode mode;
  int i, j;

  child1 = gradient ();
  child2 = ducky ();

  for (i = 0, mode = GSK_BLEND_MODE_DEFAULT; i < 4; i++)
    for (j = 0; j < 4; j++, mode++)
      {
        GskRenderNode *b;
        graphene_matrix_t matrix;

        b = gsk_blend_node_new (child1, child2, mode);
        graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { i * 110, j * 110, 0 });
        blend[mode] = gsk_transform_node_new (b, &matrix);
        gsk_render_node_unref (b);
      }

  gsk_render_node_unref (child1);
  gsk_render_node_unref (child2);

  container = gsk_container_node_new (blend, 16);

  for (i = 0; i < 16; i++)
    gsk_render_node_unref (blend[i]);

  return container;
}

static GskRenderNode *
cross_fade (void)
{
  GskRenderNode *child1;
  GskRenderNode *child2;
  GskRenderNode *transform;
  GskRenderNode *container;
  graphene_matrix_t matrix;

  child1 = cairo ();
  child2 = cairo2 ();

  graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { 50, 50, 0 });
  transform = gsk_transform_node_new (child2, &matrix);
  gsk_render_node_unref (child2);
  child2 = transform;

  container = gsk_cross_fade_node_new (child1, child2, 0.5);

  gsk_render_node_unref (child1);
  gsk_render_node_unref (child2);

  return container;
}

static GskRenderNode *
cross_fades (void)
{
  GskRenderNode *child1;
  GskRenderNode *child2;
  GskRenderNode *node;
  GskRenderNode *nodes[5];
  GskRenderNode *container;
  graphene_matrix_t matrix;
  int i;

  child1 = cairo2 ();
  child2 = ducky ();

  for (i = 0; i < 5; i++)
    {
      node = gsk_cross_fade_node_new (child1, child2, i / 4.0);
      graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { i* 210, 0, 0 });
      nodes[i] = gsk_transform_node_new (node, &matrix);
      gsk_render_node_unref (node);
    }

  gsk_render_node_unref (child1);
  gsk_render_node_unref (child2);

  container = gsk_container_node_new (nodes, 5);

  for (i = 0; i < 5; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

static GskRenderNode *
transform (void)
{
  GskRenderNode *node;
  GskRenderNode *nodes[10];
  GskRenderNode *container;
  graphene_matrix_t scale;
  graphene_matrix_t translate;
  graphene_matrix_t matrix;
  graphene_vec3_t axis;
  graphene_vec3_init (&axis, 0.0, 0.0, 1.0);
  int i;

  node = ducky ();

  for (i = 0; i < 10; i++)
    {
      graphene_matrix_init_rotate (&scale, 20.0 * i, &axis);
      graphene_matrix_init_translate (&translate, &(const graphene_point3d_t) { i* 110, 0, 0 });
      graphene_matrix_multiply (&scale, &translate, &matrix);
      nodes[i] = gsk_transform_node_new (node, &matrix);
    }

  container = gsk_container_node_new (nodes, 5);

  for (i = 0; i < 10; i++)
    gsk_render_node_unref (nodes[i]);

  gsk_render_node_unref (node);

  return container;
}

static GskRenderNode *
opacity (void)
{
  GskRenderNode *child;
  GskRenderNode *node;
  GskRenderNode *nodes[5];
  GskRenderNode *container;
  graphene_matrix_t matrix;
  int i;

  child = ducky ();

  for (i = 0; i < 5; i++)
    {
      node = gsk_opacity_node_new (child, i / 4.0);
      graphene_matrix_init_translate (&matrix, &(const graphene_point3d_t) { i* 210, 0, 0 });
      nodes[i] = gsk_transform_node_new (node, &matrix);
      gsk_render_node_unref (node);
    }

  gsk_render_node_unref (child);

  container = gsk_container_node_new (nodes, 5);

  for (i = 0; i < 5; i++)
    gsk_render_node_unref (nodes[i]);

  return container;
}

static GskRenderNode *
color_matrix1 (void)
{
  const int N = 5;
  GskRenderNode *container_node;
  GskRenderNode *cairo_node = cairo ();
  GskRenderNode *n;
  GskRenderNode *child_nodes[N];
  graphene_matrix_t matrix;
  graphene_vec4_t offset;
  graphene_matrix_t transform;
  float cairo_width = 150;
  graphene_rect_t bounds;

  gsk_render_node_get_bounds (cairo_node, &bounds);
  cairo_width = bounds.size.width;

  /* First a cairo node inside a color matrix node, where the color matrix node doesn't do anything. */
  graphene_matrix_init_identity (&matrix);
  offset = *graphene_vec4_zero ();
  child_nodes[0] = gsk_color_matrix_node_new (cairo_node, &matrix, &offset);

  /* Now a color matrix node that actually does something. Inside a transform node. */
  offset = *graphene_vec4_zero ();
  graphene_matrix_init_scale (&matrix, 0.3, 0.3, 0.3); /* Should make the node darker */
  n = gsk_color_matrix_node_new (cairo_node, &matrix, &offset);
  graphene_matrix_init_translate (&transform, &GRAPHENE_POINT3D_INIT (cairo_width, 0, 0));
  child_nodes[1] = gsk_transform_node_new (n, &transform);

  /* Same as above, but this time we stuff the transform node in the color matrix node, and not vice versa */
  offset = *graphene_vec4_zero ();
  graphene_matrix_init_scale (&matrix, 0.3, 0.3, 0.3);
  graphene_matrix_init_translate (&transform, &GRAPHENE_POINT3D_INIT (2 * cairo_width, 0, 0));
  n = gsk_transform_node_new (cairo_node, &transform);
  child_nodes[2] = gsk_color_matrix_node_new (n, &matrix, &offset);

  /* Color matrix inside color matrix, one reversing the other's effect */
  {
    graphene_matrix_t inner_matrix;
    graphene_vec4_t inner_offset = *graphene_vec4_zero ();
    GskRenderNode *inner_color_matrix_node;

    graphene_matrix_init_scale (&inner_matrix, 0.5, 0.5, 0.5);
    inner_color_matrix_node = gsk_color_matrix_node_new (cairo_node, &inner_matrix, &inner_offset);

    graphene_matrix_init_scale (&matrix, 2, 2, 2);
    n = gsk_color_matrix_node_new (inner_color_matrix_node, &matrix, &offset);
    graphene_matrix_init_translate (&transform, &GRAPHENE_POINT3D_INIT (3 * cairo_width, 0, 0));
    child_nodes[3] = gsk_transform_node_new (n, &transform);
  }

  /* Color matrix in color matrix in transform */
  {
    graphene_matrix_t inner_matrix;
    graphene_vec4_t inner_offset = *graphene_vec4_zero ();
    GskRenderNode *inner_color_matrix_node;

    graphene_matrix_init_scale (&inner_matrix, 0.5, 0.5, 0.5);
    inner_color_matrix_node = gsk_color_matrix_node_new (cairo_node, &inner_matrix, &inner_offset);

    graphene_matrix_init_scale (&matrix, 2, 2, 2);
    offset = *graphene_vec4_zero ();
    n = gsk_color_matrix_node_new (inner_color_matrix_node, &matrix, &offset);
    graphene_matrix_init_scale (&transform, 1, 1, 1);
    graphene_matrix_rotate_z (&transform, 350);
    graphene_matrix_translate (&transform, &GRAPHENE_POINT3D_INIT (4 * cairo_width, 0, 0));

    child_nodes[4] = gsk_transform_node_new (n, &transform);
  }

  container_node = gsk_container_node_new (child_nodes, N);

  return container_node;
}

static const struct {
  const char *name;
  GskRenderNode * (* func) (void);
} functions[] = {
  { "colors.node", colors },
  { "cairo.node", cairo },
  { "repeat.node", repeat },
  { "blendmode.node", blendmode },
  { "cross-fade.node", cross_fade },
  { "blendmodes.node", blendmodes },
  { "cross-fades.node", cross_fades },
  { "transform.node", transform },
  { "opacity.node", opacity },
  { "color-matrix1.node", color_matrix1},
};

/*** test setup ***/

char *
file_replace_extension (const char *old_file,
                        const char *old_ext,
                        const char *new_ext)
{
  GString *file = g_string_new (NULL);

  if (g_str_has_suffix (old_file, old_ext))
    g_string_append_len (file, old_file, strlen (old_file) - strlen (old_ext));
  else
    g_string_append (file, old_file);

  g_string_append (file, new_ext);

  return g_string_free (file, FALSE);
}

static char *
get_output_file (const char *file,
                 const char *orig_ext,
                 const char *new_ext)
{
  const char *dir;
  char *result, *base;
  char *name;

  dir = g_get_tmp_dir ();
  base = g_path_get_basename (file);
  name = file_replace_extension (base, orig_ext, new_ext);

  result = g_strconcat (dir, G_DIR_SEPARATOR_S, name, NULL);

  g_free (base);
  g_free (name);

  return result;
}

static void
save_image (cairo_surface_t *surface,
            const char      *test_name,
            const char      *extension)
{
  char *filename = get_output_file (test_name, ".node", extension);

  g_test_message ("Storing test result image at %s", filename);
  g_assert (cairo_surface_write_to_png (surface, filename) == CAIRO_STATUS_SUCCESS);
  g_free (filename);
}

static void
load_node_file (GFile *file, gboolean generate)
{
  char *node_file;
  GError *error = NULL;
  char *contents;
  gsize len;
  GBytes *bytes;
  GskRenderNode *node;
  GskRenderer *renderer;
  GdkWindow *window;
  GdkTexture *texture = NULL;
  cairo_surface_t *surface;
  char *png_file;
  cairo_surface_t *ref_surface;
  cairo_surface_t *diff_surface;
  const char *ext;

  node_file = g_file_get_path (file);

  if (!g_file_get_contents (node_file, &contents, &len, &error))
    {
      g_test_message ("Could not open node file: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }
  bytes = g_bytes_new_take (contents, len);
  node = gsk_render_node_deserialize (bytes, &error);
  g_bytes_unref (bytes);

  if (node == NULL)
    {
      g_test_message ("Invalid node file: %s\n", error->message);
      g_clear_error (&error);
      g_test_fail ();
      return;
    }

  window = gdk_window_new_toplevel (gdk_display_get_default(), 10 , 10);
  renderer = gsk_renderer_new_for_window (window);
  texture = gsk_renderer_render_texture (renderer, node, NULL);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gdk_texture_get_width (texture),
                                        gdk_texture_get_height (texture));
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  cairo_surface_mark_dirty (surface);

  if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskVulkanRenderer") == 0)
    ext = ".vulkan.png";
  else if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskGLRenderer") == 0)
    ext = ".gl.png";
  else if (strcmp (G_OBJECT_TYPE_NAME (renderer), "GskCairoRenderer") == 0)
    ext = ".cairo.png";
  else
    ext = ".png";

  g_object_unref (texture);
  g_object_unref (window);
  gsk_renderer_unrealize (renderer);
  g_object_unref (renderer);
  gdk_window_destroy (window);

  gsk_render_node_unref (node);

  if (generate)
    {
      cairo_status_t status;
      char *out_file;

      out_file = file_replace_extension (node_file, ".node", ".png");

      status = cairo_surface_write_to_png (surface, out_file);
      cairo_surface_destroy (surface);
      if (status != CAIRO_STATUS_SUCCESS)
        {
          g_print ("Failed to save png file %s: %s\n", out_file, cairo_status_to_string (status));
          exit (1);
        }
      g_free (out_file);
      return;
    }

  png_file = file_replace_extension (node_file, ".node", ext);
  if (!g_file_test (png_file, G_FILE_TEST_EXISTS))
    {
      g_free (png_file);
      png_file = file_replace_extension (node_file, ".node", ".png");
    }

  g_test_message ("using reference image %s", png_file);
  ref_surface = cairo_image_surface_create_from_png (png_file);
  diff_surface = reftest_compare_surfaces (surface, ref_surface);

  save_image (surface, node_file, ".out.png");
  save_image (ref_surface, node_file, ".ref.png");
  if (diff_surface)
    {
      save_image (diff_surface, node_file, ".diff.png");
      g_test_fail ();
    }

  cairo_surface_destroy (surface);
  cairo_surface_destroy (ref_surface);
  if (diff_surface)
    cairo_surface_destroy (diff_surface);

  g_free (png_file);
  g_free (node_file);
}

static void
test_node_file (GFile *file)
{
  load_node_file (file, FALSE);
}

static void
add_test_for_file (GFile *file)
{
  char *path;

  path = g_file_get_path (file);

  g_test_add_vtable (path,
                     0,
                     g_object_ref (file),
                     NULL,
                     (GTestFixtureFunc) test_node_file,
                     (GTestFixtureFunc) g_object_unref);

  g_free (path);
}

static int
compare_files (gconstpointer a, gconstpointer b)
{
  GFile *file1 = G_FILE (a);
  GFile *file2 = G_FILE (b);
  char *path1, *path2;
  int result;

  path1 = g_file_get_path (file1);
  path2 = g_file_get_path (file2);

  result = strcmp (path1, path2);

  g_free (path1);
  g_free (path2);

  return result;
}

static void
add_tests_for_files_in_directory (GFile *dir)
{
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GList *files;
  GError *error = NULL;

  enumerator = g_file_enumerate_children (dir, G_FILE_ATTRIBUTE_STANDARD_NAME, 0, NULL, &error);
  g_assert_no_error (error);
  files = NULL;

  while ((info = g_file_enumerator_next_file (enumerator, NULL, &error)))
    {
      const char *filename;

      filename = g_file_info_get_name (info);

      if (!g_str_has_suffix (filename, ".node") ||
          g_str_has_suffix (filename, ".png"))
        {
          g_object_unref (info);
          continue;
        }

      files = g_list_prepend (files, g_file_get_child (dir, filename));

      g_object_unref (info);
    }

  g_assert_no_error (error);
  g_object_unref (enumerator);

  files = g_list_sort (files, compare_files);
  g_list_foreach (files, (GFunc) add_test_for_file, NULL);
  g_list_free_full (files, g_object_unref);
}

static void
generate (const char *name)
{
  int i;
  GError *error = NULL;
  gboolean wrote_file = FALSE;

  for (i = 0; i < G_N_ELEMENTS (functions); i++)
    {
      if (strcmp (name, functions[i].name) == 0)
        {
          GskRenderNode *node = functions[i].func ();
          if (!gsk_render_node_write_to_file (node, name, &error))
            {
              g_print ("Error writing '%s': %s\n", name, error->message);
              g_clear_error (&error);
              exit (1);
            }
          else
            {
              GFile *file = g_file_new_for_commandline_arg (name);
              load_node_file (file, TRUE);
              g_object_unref (file);
            }

          gsk_render_node_unref (node);
          wrote_file = TRUE;
          break;
        }
    }

  if (!wrote_file)
    g_warning ("Could not generate %s", name);
}

int
main (int argc, char **argv)
{
  gtk_test_init (&argc, &argv);

  if (argc < 2)
    {
      const char *basedir;
      GFile *dir;

      basedir = g_test_get_dir (G_TEST_DIST);
      dir = g_file_new_for_path (basedir);
      add_tests_for_files_in_directory (dir);

      g_object_unref (dir);
    }
  else if (strcmp (argv[1], "--generate") == 0)
    {
      if (argc >= 3)
        generate (argv[2]);
    }
  else
    {
      guint i;

      for (i = 1; i < argc; i++)
        {
          GFile *file = g_file_new_for_commandline_arg (argv[i]);
          add_test_for_file (file);
          g_object_unref (file);
        }
    }

  return g_test_run ();
}
