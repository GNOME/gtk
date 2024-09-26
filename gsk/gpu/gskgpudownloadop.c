#include "config.h"

#include "gskgpudownloadopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskglimageprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuprintprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskgpucacheprivate.h"
#include "gskvulkanbufferprivate.h"
#include "gskvulkanframeprivate.h"
#include "gskvulkanimageprivate.h"
#endif

#include <glib/gstdio.h>

#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkdmabuftexturebuilderprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkglcontextprivate.h"

#ifdef HAVE_DMABUF
#include <glib-unix.h>
#include <linux/dma-buf.h>
#endif

typedef struct _GskGpuDownloadOp GskGpuDownloadOp;

typedef void (* GdkGpuDownloadOpCreateFunc) (GskGpuDownloadOp *);

struct _GskGpuDownloadOp
{
  GskGpuOp op;

  GskGpuImage *image;
  GdkColorState *color_state;
  gboolean allow_dmabuf;
  GdkGpuDownloadOpCreateFunc create_func;
  GdkTexture **texture;

  GskGpuBuffer *buffer;
#ifdef GDK_RENDERING_VULKAN
  VkSemaphore vk_semaphore;
#endif
};

static void
gsk_gpu_download_op_finish (GskGpuOp *op)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;

  if (self->create_func)
    self->create_func (self);

  g_object_unref (self->image);
  g_clear_object (&self->buffer);
}

static void
gsk_gpu_download_op_print (GskGpuOp    *op,
                           GskGpuFrame *frame,
                           GString     *string,
                           guint        indent)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;

  gsk_gpu_print_op (string, indent, "download");
  gsk_gpu_print_image (string, self->image);
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN

static GskGpuBuffer *
gsk_gpu_download_vk_start (GskGpuFrame           *frame,
                           GskVulkanCommandState *state,
                           GskGpuImage           *image)
{
  gsize width, height, stride;
  GskGpuBuffer *buffer;

  width = gsk_gpu_image_get_width (image);
  height = gsk_gpu_image_get_height (image);
  stride = width * gdk_memory_format_bytes_per_pixel (gsk_gpu_image_get_format (image));
  buffer = gsk_vulkan_buffer_new_read (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                       height * stride);

  gsk_vulkan_image_transition (GSK_VULKAN_IMAGE (image),
                               state->semaphores,
                               state->vk_command_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               VK_ACCESS_TRANSFER_READ_BIT);

  vkCmdCopyImageToBuffer (state->vk_command_buffer,
                          gsk_vulkan_image_get_vk_image (GSK_VULKAN_IMAGE (image)),
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (buffer)),
                          1,
                          (VkBufferImageCopy[1]) {
                               {
                                   .bufferOffset = 0,
                                   .bufferRowLength = width,
                                   .bufferImageHeight = height,
                                   .imageSubresource = {
                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1
                                   },
                                   .imageOffset = {
                                       .x = 0,
                                       .y = 0,
                                       .z = 0
                                   },
                                   .imageExtent = {
                                       .width = width,
                                       .height = height,
                                       .depth = 1
                                   }
                               }
                          });

  vkCmdPipelineBarrier (state->vk_command_buffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT,
                        0,
                        0, NULL,
                        1, &(VkBufferMemoryBarrier) {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                            .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .buffer = gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (buffer)),
                            .offset = 0,
                            .size = VK_WHOLE_SIZE,
                        },
                        0, NULL);

  return buffer;
}

#ifdef HAVE_DMABUF
/* The code needs to run here because vkGetSemaphoreFdKHR() may
 * only be called after the semaphore has been submitted via
 * vkQueueSubmit().
 */
static void
gsk_gpu_download_op_vk_sync_semaphore (GskGpuDownloadOp *self)
{
  PFN_vkGetSemaphoreFdKHR func_vkGetSemaphoreFdKHR;
  GdkDisplay *display;
  int fd, sync_file_fd;

  /* Don't look at where I store my variables plz */
  display = gdk_dmabuf_texture_get_display (GDK_DMABUF_TEXTURE (*self->texture));
  fd = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (*self->texture))->planes[0].fd;
  func_vkGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR) vkGetDeviceProcAddr (display->vk_device, "vkGetSemaphoreFdKHR");

  /* vkGetSemaphoreFdKHR implicitly resets the semaphore.
   * But who cares, we're about to delete it. */
  if (GSK_VK_CHECK (func_vkGetSemaphoreFdKHR, display->vk_device,
                                              &(VkSemaphoreGetFdInfoKHR) {
                                                  .sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR,
                                                  .semaphore = self->vk_semaphore,
                                                  .handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                                             },
                                             &sync_file_fd) == VK_SUCCESS)
    {
      gdk_dmabuf_import_sync_file (fd, DMA_BUF_SYNC_WRITE, sync_file_fd);
      
      close (sync_file_fd);
    }

  vkDestroySemaphore (display->vk_device, self->vk_semaphore, NULL);
}
#endif

static void
gsk_gpu_download_op_vk_create (GskGpuDownloadOp *self)
{
  GBytes *bytes;
  guchar *data;
  gsize width, height, stride;
  GdkMemoryFormat format;
  GdkMemoryTextureBuilder *builder;

  data = gsk_gpu_buffer_map (self->buffer);
  width = gsk_gpu_image_get_width (self->image);
  height = gsk_gpu_image_get_height (self->image);
  format = gsk_gpu_image_get_format (self->image);
  stride = width * gdk_memory_format_bytes_per_pixel (format);
  bytes = g_bytes_new (data, stride * height);

  builder = gdk_memory_texture_builder_new ();
  gdk_memory_texture_builder_set_width (builder, width);
  gdk_memory_texture_builder_set_height (builder, height);
  gdk_memory_texture_builder_set_format (builder, format);
  gdk_memory_texture_builder_set_bytes (builder, bytes);
  gdk_memory_texture_builder_set_stride (builder, stride);
  gdk_memory_texture_builder_set_color_state (builder, self->color_state);
  *self->texture = gdk_memory_texture_builder_build (builder);

  g_object_unref (builder);
  g_bytes_unref (bytes);
  gsk_gpu_buffer_unmap (self->buffer, 0);
}

static GskGpuOp *
gsk_gpu_download_op_vk_command (GskGpuOp              *op,
                                GskGpuFrame           *frame,
                                GskVulkanCommandState *state)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;

#ifdef HAVE_DMABUF
  if (self->allow_dmabuf)
    *self->texture = gsk_vulkan_image_to_dmabuf_texture (GSK_VULKAN_IMAGE (self->image),
                                                        self->color_state);
  if (*self->texture)
    {
      GskGpuDevice *device = gsk_gpu_frame_get_device (frame);
      GskGpuCache *cache = gsk_gpu_device_get_cache (device);
      VkDevice vk_device = gsk_vulkan_device_get_vk_device (GSK_VULKAN_DEVICE (device));

      gsk_gpu_cache_cache_texture_image (cache, *self->texture, self->image, NULL);

      if (gsk_vulkan_device_has_feature (GSK_VULKAN_DEVICE (device), GDK_VULKAN_FEATURE_SEMAPHORE_EXPORT))
        {
          GSK_VK_CHECK (vkCreateSemaphore, vk_device,
                                           &(VkSemaphoreCreateInfo) {
                                               .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  			                       .pNext = &(VkExportSemaphoreCreateInfo) {
                                                   .sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO,
                                                   .handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
			                       },
                                           },
                                           NULL,
                                           &self->vk_semaphore);
          gsk_vulkan_semaphores_add_signal (state->semaphores, self->vk_semaphore);

          self->create_func = gsk_gpu_download_op_vk_sync_semaphore;
        }

      return op->next;
    }
#endif

  self->buffer = gsk_gpu_download_vk_start (frame, state, self->image);
  self->create_func = gsk_gpu_download_op_vk_create;

  return op->next;
}
#endif

typedef struct _GskGLTextureData GskGLTextureData;

struct _GskGLTextureData
{
  GdkGLContext *context;
  GLuint texture_id;
  GLsync sync;
};

static void
gsk_gl_texture_data_free (gpointer user_data)
{
  GskGLTextureData *data = user_data;

  gdk_gl_context_make_current (data->context);

  /* can't use g_clear_pointer() on glDeleteSync(), see MR !7294 */
  if (data->sync)
    {
      glDeleteSync (data->sync);
      data->sync = NULL;
    }

  glDeleteTextures (1, &data->texture_id);
  g_object_unref (data->context);

  g_free (data);
}

#ifdef HAVE_DMABUF
typedef struct
{
  GdkDmabuf dmabuf;
} Texture;

static void
release_dmabuf_texture (gpointer data)
{
  Texture *texture = data;

  gdk_dmabuf_close_fds (&texture->dmabuf);
  g_free (texture);
}
#endif

static GskGpuOp *
gsk_gpu_download_op_gl_command (GskGpuOp          *op,
                                GskGpuFrame       *frame,
                                GskGLCommandState *state)
{
  GskGpuDownloadOp *self = (GskGpuDownloadOp *) op;
  GdkGLTextureBuilder *builder;
  GskGLTextureData *data;
  guint texture_id;

  texture_id = gsk_gl_image_get_texture_id (GSK_GL_IMAGE (self->image));

#ifdef HAVE_DMABUF
  if (self->allow_dmabuf)
    {
      GdkGLContext *context;
      Texture *texture;

      context = GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame));
      texture = g_new0 (Texture, 1);

      if (gdk_gl_context_export_dmabuf (context, texture_id, &texture->dmabuf))
        {
          GdkDmabufTextureBuilder *db;

          db = gdk_dmabuf_texture_builder_new ();
          gdk_dmabuf_texture_builder_set_display (db, gdk_gl_context_get_display (context));
          gdk_dmabuf_texture_builder_set_dmabuf (db, &texture->dmabuf);
          gdk_dmabuf_texture_builder_set_premultiplied (db, gdk_memory_format_get_premultiplied (gsk_gpu_image_get_format (self->image)));
          gdk_dmabuf_texture_builder_set_width (db, gsk_gpu_image_get_width (self->image));
          gdk_dmabuf_texture_builder_set_height (db, gsk_gpu_image_get_height (self->image));
          gdk_dmabuf_texture_builder_set_color_state (db, self->color_state);

          *self->texture = gdk_dmabuf_texture_builder_build (db, release_dmabuf_texture, texture, NULL);

          g_object_unref (db);

          if (*self->texture)
            return op->next;

          gdk_dmabuf_close_fds (&texture->dmabuf);
        }

      g_free (texture);
    }
#endif

  data = g_new (GskGLTextureData, 1);
  /* Don't use the renderer context for the texture,
   * the texture might survive the frame and its surface */
  data->context = g_object_ref (gdk_display_get_gl_context (gsk_gpu_device_get_display (gsk_gpu_frame_get_device (frame))));
  data->texture_id = texture_id;
  data->sync = glFenceSync (GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

  builder = gdk_gl_texture_builder_new ();
  gdk_gl_texture_builder_set_context (builder, data->context);
  gdk_gl_texture_builder_set_id (builder, data->texture_id);
  gdk_gl_texture_builder_set_format (builder, gsk_gpu_image_get_format (self->image));
  gdk_gl_texture_builder_set_color_state (builder, self->color_state);
  gdk_gl_texture_builder_set_width (builder, gsk_gpu_image_get_width (self->image));
  gdk_gl_texture_builder_set_height (builder, gsk_gpu_image_get_height (self->image));
  gdk_gl_texture_builder_set_sync (builder, data->sync);

  *self->texture = gdk_gl_texture_builder_build (builder,
                                                 gsk_gl_texture_data_free,
                                                 data);

  gsk_gpu_image_toggle_ref_texture (self->image, *self->texture);
  gsk_gl_image_steal_texture_ownership (GSK_GL_IMAGE (self->image));

  g_object_unref (builder);

  return op->next;
}

static const GskGpuOpClass GSK_GPU_DOWNLOAD_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuDownloadOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_download_op_finish,
  gsk_gpu_download_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_download_op_vk_command,
#endif
  gsk_gpu_download_op_gl_command
};

void
gsk_gpu_download_op (GskGpuFrame        *frame,
                     GskGpuImage        *image,
                     GdkColorState      *color_state,
                     GdkTexture        **out_texture)
{
  GskGpuDownloadOp *self;

  g_assert (out_texture != NULL && *out_texture == NULL);

  self = (GskGpuDownloadOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_DOWNLOAD_OP_CLASS);

  self->image = g_object_ref (image);
  self->color_state = gdk_color_state_ref (color_state);
  self->texture = out_texture;
}

typedef struct _GskGpuDownloadIntoOp GskGpuDownloadIntoOp;

typedef void (* GdkGpuDownloadIntoOpCreateFunc) (GskGpuDownloadIntoOp *);

struct _GskGpuDownloadIntoOp
{
  GskGpuOp op;

  GdkGpuDownloadIntoOpCreateFunc create_func;
  GskGpuBuffer *buffer;

  GskGpuImage *image;
  GdkColorState *image_color_state;

  GdkMemoryFormat format;
  GdkColorState *color_state;
  guchar *data;
  gsize stride;
};

static void
gsk_gpu_download_into_op_finish (GskGpuOp *op)
{
  GskGpuDownloadIntoOp *self = (GskGpuDownloadIntoOp *) op;

  if (self->create_func)
    self->create_func (self);

  g_object_unref (self->image);
  gdk_color_state_unref (self->image_color_state);
  gdk_color_state_unref (self->color_state);
  g_clear_object (&self->buffer);
}

static void
gsk_gpu_download_into_op_print (GskGpuOp    *op,
                                GskGpuFrame *frame,
                                GString     *string,
                                guint        indent)
{
  GskGpuDownloadIntoOp *self = (GskGpuDownloadIntoOp *) op;

  gsk_gpu_print_op (string, indent, "download-into");
  gsk_gpu_print_image (string, self->image);
  g_string_append_printf (string, "%s ", gdk_color_state_get_name (self->image_color_state));
  g_string_append_printf (string, "%s ", gdk_memory_format_get_name (self->format));
  g_string_append_printf (string, "%s ", gdk_color_state_get_name (self->color_state));
  gsk_gpu_print_newline (string);
}

#ifdef GDK_RENDERING_VULKAN
static void
gsk_gpu_download_into_op_vk_create (GskGpuDownloadIntoOp *self)
{
  guchar *data;
  GdkMemoryFormat format;
  gsize width, height;

  data = gsk_gpu_buffer_map (self->buffer);
  format = gsk_gpu_image_get_format (self->image);
  width = gsk_gpu_image_get_width (self->image);
  height = gsk_gpu_image_get_height (self->image);

  gdk_memory_convert (self->data,
                      self->stride,
                      self->format,
                      self->color_state,
                      data,
                      width * gdk_memory_format_bytes_per_pixel (format),
                      format,
                      self->image_color_state,
                      width,
                      height);
}

static GskGpuOp *
gsk_gpu_download_into_op_vk_command (GskGpuOp              *op,
                                     GskGpuFrame           *frame,
                                     GskVulkanCommandState *state)
{
  GskGpuDownloadIntoOp *self = (GskGpuDownloadIntoOp *) op;

  self->buffer = gsk_gpu_download_vk_start (frame, state, self->image);
  self->create_func = gsk_gpu_download_into_op_vk_create;

  return op->next;
}
#endif

static GskGpuOp *
gsk_gpu_download_into_op_gl_command (GskGpuOp          *op,
                                     GskGpuFrame       *frame,
                                     GskGLCommandState *state)
{
  GskGpuDownloadIntoOp *self = (GskGpuDownloadIntoOp *) op;

  gdk_gl_context_download (GDK_GL_CONTEXT (gsk_gpu_frame_get_context (frame)),
                           gsk_gl_image_get_texture_id (GSK_GL_IMAGE (self->image)),
                           gsk_gpu_image_get_format (self->image),
                           self->image_color_state,
                           self->data,
                           self->stride,
                           self->format,
                           self->color_state,
                           gsk_gpu_image_get_width (self->image),
                           gsk_gpu_image_get_height (self->image));

  return op->next;
}

static const GskGpuOpClass GSK_GPU_DOWNLOAD_INTO_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuDownloadIntoOp),
  GSK_GPU_STAGE_COMMAND,
  gsk_gpu_download_into_op_finish,
  gsk_gpu_download_into_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_download_into_op_vk_command,
#endif
  gsk_gpu_download_into_op_gl_command
};

void
gsk_gpu_download_into_op (GskGpuFrame     *frame,
                          GskGpuImage     *image,
                          GdkColorState   *image_color_state,
                          GdkMemoryFormat  format,
                          GdkColorState   *color_state,
                          guchar          *data,
                          gsize            stride)
{
  GskGpuDownloadIntoOp *self;

  self = (GskGpuDownloadIntoOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_DOWNLOAD_INTO_OP_CLASS);

  self->image = g_object_ref (image);
  self->image_color_state = gdk_color_state_ref (image_color_state);
  self->format = format;
  self->color_state = gdk_color_state_ref (color_state);
  self->data = data;
  self->stride = stride;
}

