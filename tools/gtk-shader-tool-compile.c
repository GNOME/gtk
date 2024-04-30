/*  Copyright 2024 Red Hat, Inc.
 *
 * GTK+ is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GTK+; see the file COPYING.  If not,
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

#include "gdk/gdkvulkancontextprivate.h"
#include "gsk/gpu/gskgputypesprivate.h"
#include "gsk/gpu/gskvulkandeviceprivate.h"
#include "gsk/gpu/gskgpushaderopprivate.h"

#include "gtk-shader-tool.h"

static gboolean verbose;

typedef struct {
  const GskGpuShaderOpClass *op_class;
  unsigned int variation;
  GskGpuShaderClip clip;
  GskGpuBlend blend;
  VkFormat format;

  gsize n_buffers;
  gsize n_samplers;
  gsize n_immutable_samplers;
} PipelineData;

static const char *clip_name[] = { "NONE", "RECT", "ROUNDED" };
static const char *blend_name[] = { "OVER", "ADD", "CLEAR" };

static gboolean
parse_line (const char   *line,
            PipelineData *data)
{
  char *p, *q;
  char *name;
  char *parms;
  char **split;
  int i;

  if (!g_str_has_prefix (line, "Create Vulkan pipeline ("))
    return FALSE;

  line += strlen ("Create Vulkan pipeline (");

  p = strchr (line, ' ');
  if (!p)
    return FALSE;

  name = g_strndup (line, p - line);
  data->op_class = gsk_gpu_shader_op_class_from_name (name);
  if (!data->op_class)
    {
      g_printerr ("%s\n", g_strdup_printf (_("No such shader: %s"), name));
      return FALSE;
    }
  g_free (name);
  p++;

  p = strchr (p, ' ');
  if (!p)
    return FALSE;

  q = strstr (p, ") for layout (");
  if (!q)
    return FALSE;

  parms = g_strndup (p, q - p);
  split = g_strsplit (parms, "/", 0);
  g_free (parms);

  if (g_strv_length (split) != 4)
    {
      g_strfreev (split);
      return FALSE;
    }

  data->variation = (unsigned int) g_ascii_strtoull (split[0], NULL, 10);

  for (i = 0; i < G_N_ELEMENTS (clip_name); i++)
    {
      if (strcmp (split[1], clip_name[i]) == 0)
        {
          data->clip = i;
          break;
        }
    }
  if (i == G_N_ELEMENTS (clip_name))
    {
      g_printerr ("%s\n", g_strdup_printf (_("No such clip: %s"), split[1]));
      g_strfreev (split);
      return FALSE;
    }

  for (i = 0; i < G_N_ELEMENTS (blend_name); i++)
    {
      if (strcmp (split[2], blend_name[i]) == 0)
        {
          data->blend = i;
          break;
        }
    }
  if (i == G_N_ELEMENTS (blend_name))
    {
      g_printerr ("%s\n", g_strdup_printf (_("No such blend: %s"), split[2]));
      g_strfreev (split);
      return FALSE;
    }

  data->format = (guint) g_ascii_strtoull (split[3], NULL, 10);

  g_strfreev (split);

  p = q + strlen (") for layout (");
  q = strstr (p, ")");
  if (!q)
    return FALSE;

  parms = g_strndup (p, q - p);
  split = g_strsplit (parms, "/", 0);
  g_free (parms);

  if (g_strv_length (split ) != 3)
    {
      g_strfreev (split);
      return FALSE;
    }

  data->n_buffers = (gsize) g_ascii_strtoull (split[0], NULL, 10);
  data->n_samplers = (gsize) g_ascii_strtoull (split[1], NULL, 10);
  data->n_immutable_samplers = (gsize) g_ascii_strtoull (split[2], NULL, 10);

  g_strfreev (split);

  return TRUE;
}

static gboolean
compile_shader (GskVulkanDevice *device,
                PipelineData    *data)
{
  GskVulkanPipelineLayout *layout;
  VkRenderPass render_pass;
  VkPipeline pipeline;

  if (data->n_immutable_samplers != 0)
    {
      g_printerr ("%s\n", _("Can't handle pipeline layouts with immutable samplers\n"));
      return FALSE;
    }

  if (verbose)
    {
      g_print ("layout: %3lu buffers %3lu samplers ",
               data->n_buffers, data->n_samplers);
      g_print ("shader: %20s, variation %u clip %7s blend %5s format %u\n",
               data->op_class->shader_name,
               data->variation,
               clip_name[data->clip],
               blend_name[data->blend],
               data->format);
    }

  layout = gsk_vulkan_device_acquire_pipeline_layout (device,
                                                      NULL,
                                                      data->n_immutable_samplers,
                                                      data->n_samplers,
                                                      data->n_buffers);

  render_pass = gsk_vulkan_device_get_vk_render_pass (device,
                                                      data->format,
                                                      VK_IMAGE_LAYOUT_PREINITIALIZED,
                                                      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  pipeline = gsk_vulkan_device_get_vk_pipeline (device,
                                                layout,
                                                data->op_class,
                                                data->variation,
                                                data->clip,
                                                data->blend,
                                                data->format,
                                                render_pass);

  gsk_vulkan_device_release_pipeline_layout (device, layout);

  return pipeline != VK_NULL_HANDLE;
}

static void
compile_shaders_from_file (GskVulkanDevice *device,
                           const char      *filename)
{
  char *buffer;
  gsize size;
  GError *error = NULL;
  char **lines;
  PipelineData data;

  if (!g_file_get_contents (filename, &buffer, &size, &error))
    {
      char *msg = g_strdup_printf (_("Failed to read %s"), filename);
      g_printerr ("%s: %s\n", msg, error->message);
      exit (1);
    }

  lines = g_strsplit (buffer, "\n", 0);

  for (int i = 0; lines[i]; i++)
    {
      lines[i] = g_strstrip (lines[i]);

      if (strlen (lines[i]) == 0)
        continue;

      if (!parse_line (lines[i], &data))
        {
          char *msg = g_strdup_printf (_("Could not parse line %d: %s"), i + 1, lines[i]);
          g_printerr ("%s\n", msg);
          exit (1);
        }

      if (!compile_shader (device, &data))
        {
          char *msg = g_strdup_printf (_("Failed to compile shader for line %d: %s"), i + 1, lines[i]);
          g_printerr ("%s\n", msg);
          exit (1);
        }
    }

  g_strfreev (lines);
  g_free (buffer);
}

void
do_compile (int          *argc,
            const char ***argv)
{
  GError *error = NULL;
  char **args = NULL;
  GOptionContext *context;
  const GOptionEntry entries[] = {
    { "verbose", 0, 0, G_OPTION_ARG_NONE, &verbose, N_("Be verbose"), NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &args, NULL, N_("FILE") },
    { NULL, }
  };
  GdkDisplay *display;
  GskGpuDevice *device;

  g_set_prgname ("gtk4-shader-tool compile");
  context = g_option_context_new (NULL);
  g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_set_summary (context, _("Compile shaders."));

  if (!g_option_context_parse (context, argc, (char ***)argv, &error))
    {
      g_printerr ("%s\n", error->message);
      g_error_free (error);
      exit (1);
    }

  g_option_context_free (context);

  if (args == NULL)
    {
      g_printerr ("%s\n", _("No file specified."));
      exit (1);
    }

  display = gdk_display_get_default ();
  device = gsk_vulkan_device_get_for_display (display, &error);
  if (device == NULL)
    {
      g_printerr ("%s: %s\n", _("Failed to get Vulkan device"), error->message);
      exit (1);
    }

  for (int i = 0; args[i]; i++)
    compile_shaders_from_file (GSK_VULKAN_DEVICE (device), args[i]);

  if (gdk_display_vulkan_pipeline_cache_save (display))
    {
      GFile *file = gdk_display_vulkan_pipeline_cache_file (display);

      g_print (_("Pipeline cache in %s\n"), g_file_peek_path (file));

      g_object_unref (file);
    }

  g_strfreev (args);
}
