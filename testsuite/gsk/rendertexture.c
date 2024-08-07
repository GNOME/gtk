#include <gtk/gtk.h>

enum {
  GDK_COLOR_STATE_SRGB,
  GDK_COLOR_STATE_SRGB_LINEAR,
  GDK_COLOR_STATE_REC2100_PQ,
  GDK_COLOR_STATE_REC2100_LINEAR,
};

typedef struct {
  const char *renderer;
  const char *debug;
  gboolean hdr_content;
  guint expected;
} TextureTest;

static TextureTest tests[] = {
  { "ngl",    "",           0, GDK_COLOR_STATE_SRGB           },
  { "ngl",    "",           1, GDK_COLOR_STATE_REC2100_PQ     },
  { "ngl",    "linear",     0, GDK_COLOR_STATE_SRGB           },
  { "ngl",    "linear",     1, GDK_COLOR_STATE_REC2100_LINEAR },
  { "ngl",    "hdr",        0, GDK_COLOR_STATE_REC2100_PQ     },
  { "ngl",    "hdr",        1, GDK_COLOR_STATE_REC2100_PQ     },
  { "ngl",    "hdr:linear", 0, GDK_COLOR_STATE_REC2100_LINEAR },
  { "ngl",    "hdr:linear", 1, GDK_COLOR_STATE_REC2100_LINEAR },
  { "vulkan", "",           0, GDK_COLOR_STATE_SRGB           },
  { "vulkan", "",           1, GDK_COLOR_STATE_REC2100_PQ     },
  { "vulkan", "linear",     0, GDK_COLOR_STATE_SRGB           },
  { "vulkan", "linear",     1, GDK_COLOR_STATE_REC2100_LINEAR },
  { "vulkan", "hdr",        0, GDK_COLOR_STATE_REC2100_PQ     },
  { "vulkan", "hdr",        1, GDK_COLOR_STATE_REC2100_PQ     },
  { "vulkan", "hdr:linear", 0, GDK_COLOR_STATE_REC2100_LINEAR },
  { "vulkan", "hdr:linear", 1, GDK_COLOR_STATE_REC2100_LINEAR },
};

static GdkColorState *
get_color_state (guint id)
{
  switch (id)
    {
    case GDK_COLOR_STATE_SRGB:           return gdk_color_state_get_srgb ();
    case GDK_COLOR_STATE_SRGB_LINEAR:    return gdk_color_state_get_srgb_linear ();
    case GDK_COLOR_STATE_REC2100_PQ:     return gdk_color_state_get_rec2100_pq ();
    case GDK_COLOR_STATE_REC2100_LINEAR: return gdk_color_state_get_rec2100_linear ();
    default:                             g_assert_not_reached ();
    }
}

static const char *
color_state_name (GdkColorState *cs)
{
  if (cs == gdk_color_state_get_srgb ())
    return "srgb";
  else if (cs == gdk_color_state_get_srgb_linear ())
    return "srgb-linear";
  else if (cs == gdk_color_state_get_rec2100_pq ())
    return "rec2100-pq";
  else if (cs == gdk_color_state_get_rec2100_linear ())
    return "rec2100-linear";
  else
    return "???";
}

static void
test_render_texture (gconstpointer testdata)
{
  const TextureTest *test = testdata;

  if (g_test_subprocess ())
    {
      GError *error = NULL;
      const char *text;
      GBytes *bytes;
      GskRenderNode *node;
      GskRenderer *renderer;
      GdkTexture *texture;
      GdkTextureDownloader *downloader;
      GdkColorState *expected = get_color_state (test->expected);

      gtk_init ();

      text = test->hdr_content
             ? "color { color: color(rec2100-pq 1 0.5 0); }"
             : "color { color: color(srgb 0 0.5 1); }";

      bytes = g_bytes_new_static (text, strlen (text));
      node = gsk_render_node_deserialize (bytes, NULL, NULL);
      g_bytes_unref (bytes);

      g_assert_nonnull (node);

      if (strcmp (test->renderer, "ngl") == 0)
        renderer = gsk_ngl_renderer_new ();
      else if (strcmp (test->renderer, "vulkan") == 0)
        renderer = gsk_vulkan_renderer_new ();
      else
        g_assert_not_reached ();

      gsk_renderer_realize_for_display (renderer, gdk_display_get_default (), &error);
      g_assert_no_error (error);

      texture = gsk_renderer_render_texture (renderer, node, &GRAPHENE_RECT_INIT (0, 0, 1, 1));

      if (!gdk_color_state_equal (expected, gdk_texture_get_color_state (texture)))
        {
          g_print ("test: expected %s, got %s\n",
                    color_state_name (expected),
                    color_state_name (gdk_texture_get_color_state (texture)));
          exit (1);
        }
      else
        {
          g_print ("test: got color state %s\n",
                   color_state_name (gdk_texture_get_color_state (texture)));
        }

      g_assert_true (gdk_texture_get_width (texture) == 1);
      g_assert_true (gdk_texture_get_height (texture) == 1);

      downloader = gdk_texture_downloader_new (texture);
      gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R32G32B32A32_FLOAT);

       /* Convert the data to the colorstate we used in the node */
      if (test->hdr_content)
        gdk_texture_downloader_set_color_state (downloader, gdk_color_state_get_rec2100_pq ());
      else
        gdk_texture_downloader_set_color_state (downloader, gdk_color_state_get_srgb ());

      float data[4];
      gdk_texture_downloader_download_into (downloader, (guchar *) data, 4 * sizeof (float));

      gdk_texture_downloader_free (downloader);

      g_print ("test: got %s content: %f %f %f %f\n",
               test->hdr_content ? "rec2100-pq" : "srgb",
               data[0], data[1], data[2], data[3]);

      if (test->hdr_content)
        {
          g_assert_cmpfloat_with_epsilon (data[0], 1, 0.005);
          g_assert_cmpfloat_with_epsilon (data[1], 0.5, 0.005);
          g_assert_cmpfloat_with_epsilon (data[2], 0, 0.005);
          g_assert_cmpfloat_with_epsilon (data[3], 1, 0.005);
        }
      else
        {
          g_assert_cmpfloat_with_epsilon (data[0], 0, 0.005);
          g_assert_cmpfloat_with_epsilon (data[1], 0.5, 0.005);
          g_assert_cmpfloat_with_epsilon (data[2], 1, 0.005);
          g_assert_cmpfloat_with_epsilon (data[3], 1, 0.005);
        }

      gsk_render_node_unref (node);
      gsk_renderer_unrealize (renderer);
      g_object_unref (renderer);

      return;
   }

  char **envp = g_get_environ ();
  const char *str = g_getenv ("GDK_DEBUG") ? g_getenv ("GDK_DEBUG") : "";
  char *str2 = g_strconcat (test->debug, test->debug[0] && str[0] ? ":" : "", str, NULL);
  envp = g_environ_setenv (g_steal_pointer (&envp), "GDK_DEBUG", str2, TRUE);
  g_free (str2);

  g_test_trap_subprocess_with_envp (NULL, (const char * const *) envp, 0, G_TEST_SUBPROCESS_INHERIT_STDOUT | G_TEST_SUBPROCESS_INHERIT_STDERR);
  if (!g_test_trap_has_passed ())
    g_test_fail ();
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  g_test_init (&argc, &argv, NULL);

  for (int i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      TextureTest *test = &tests[i];
      char *path;

      path = g_strconcat ("/rendertexture",
                          "/renderer:", test->renderer,
                          "/content:", test->hdr_content ? "hdr" : "sdr",
                          "/flags:", test->debug[0] ? test->debug : "none", NULL);
      g_test_add_data_func (path, test, test_render_texture);
      g_free (path);
    }

  return g_test_run ();
}
