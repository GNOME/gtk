#include "config.h"

#include "gskgpuuploadopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpuprintprivate.h"
#include "gskgpuutilsprivate.h"
#include "gskgldeviceprivate.h"
#include "gskglimageprivate.h"
#ifdef GDK_RENDERING_VULKAN
#include "gskvulkanbufferprivate.h"
#include "gskvulkanimageprivate.h"
#endif

#include "gdk/gdkcolorstateprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkglcontextprivate.h"
#include "gdk/gdktextureprivate.h"
#include "gsk/gskdebugprivate.h"

static GskGpuOp *
gsk_gpu_upload_op_gl_command_with_area (GskGpuOp                    *op,
                                        GskGpuFrame                 *frame,
                                        GskGpuImage                 *image,
                                        const cairo_rectangle_int_t *area,
                                        void           (* draw_func) (GskGpuOp *, guchar *, const GdkMemoryLayout *))
{
  GskGLImage *gl_image = GSK_GL_IMAGE (image);
  GdkMemoryLayout layout;
  guchar *data, *pdata;
  guint i, p, gl_format, gl_type, stride, tex_id;
  gsize width_subsample, height_subsample, bpp;

  gdk_memory_layout_init (&layout,
                          gsk_gpu_image_get_format (GSK_GPU_IMAGE (image)),
                          area->width,
                          area->height,
                          4);
  data = g_malloc (layout.size);

  draw_func (op, data, &layout);

  glActiveTexture (GL_TEXTURE0);
  
  glPixelStorei (GL_UNPACK_ALIGNMENT, gdk_memory_format_alignment (layout.format));

  for (i = 0; i < 3; i++)
    {
      tex_id = gsk_gl_image_get_texture_id (gl_image, i);
      if (tex_id == 0)
        break;

      glBindTexture (GL_TEXTURE_2D, tex_id);

      p = gdk_memory_format_get_shader_plane (layout.format,
                                              i,
                                              &width_subsample,
                                              &height_subsample,
                                              &bpp);

      gl_format = gsk_gl_image_get_gl_format (gl_image, i);
      gl_type = gsk_gl_image_get_gl_type (gl_image, i);
      stride = layout.planes[p].stride;
      pdata = data + gdk_memory_layout_offset (&layout, p, 0, 0);

      if (stride == area->width * bpp / width_subsample)
        {
          glTexSubImage2D (GL_TEXTURE_2D, 0, 
                           area->x / width_subsample, area->y / height_subsample,
                           area->width / width_subsample, area->height / height_subsample,
                           gl_format, gl_type,
                           pdata);
        }
      else if (stride % bpp == 0)
        {
          glPixelStorei (GL_UNPACK_ROW_LENGTH, stride / bpp);

          glTexSubImage2D (GL_TEXTURE_2D, 0, 
                           area->x / width_subsample, area->y / height_subsample,
                           area->width / width_subsample, area->height / height_subsample,
                           gl_format, gl_type,
                           pdata);

          glPixelStorei (GL_UNPACK_ROW_LENGTH, 0);
        }
      else
        {
          gsize y;
          for (y = 0; y < area->height / height_subsample; y++)
            {
              glTexSubImage2D (GL_TEXTURE_2D, 0, 
                               area->x / width_subsample, area->y / height_subsample + y,
                               area->width / width_subsample, 1,
                               gl_format, gl_type,
                               pdata + (y * stride));

            }
        }
    }

  glPixelStorei (GL_UNPACK_ALIGNMENT, 4);

  g_free (data);

  return op->next;
}

static GskGpuOp *
gsk_gpu_upload_op_gl_command (GskGpuOp          *op,
                              GskGpuFrame       *frame,
                              GskGpuImage       *image,
                              void (* draw_func) (GskGpuOp *, guchar *, const GdkMemoryLayout *))
{
  return gsk_gpu_upload_op_gl_command_with_area (op,
                                                 frame,
                                                 image,
                                                 &(cairo_rectangle_int_t) {
                                                     0, 0,
                                                     gsk_gpu_image_get_width (image),
                                                     gsk_gpu_image_get_height (image)
                                                 },
                                                 draw_func);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_upload_op_vk_command_with_area (GskGpuOp                    *op,
                                        GskGpuFrame                 *frame,
                                        GskVulkanCommandState       *state,
                                        GskVulkanImage              *image,
                                        const cairo_rectangle_int_t *area,
                                        void           (* draw_func) (GskGpuOp *, guchar *, const GdkMemoryLayout *),
                                        GskGpuBuffer               **buffer)
{
  const VkImageAspectFlags aspect_flags[3] = { VK_IMAGE_ASPECT_PLANE_0_BIT, VK_IMAGE_ASPECT_PLANE_1_BIT, VK_IMAGE_ASPECT_PLANE_2_BIT };
  VkBufferImageCopy buffer_image_copy[3];
  GdkMemoryLayout layout;
  guchar *data;
  gsize i, n_planes;

  gdk_memory_layout_init (&layout,
                          gsk_gpu_image_get_format (GSK_GPU_IMAGE (image)),
                          area->width,
                          area->height,
                          1);

  *buffer = gsk_vulkan_buffer_new_write (GSK_VULKAN_DEVICE (gsk_gpu_frame_get_device (frame)),
                                         layout.size);
  data = gsk_gpu_buffer_map (*buffer);

  draw_func (op, data, &layout);

  gsk_gpu_buffer_unmap (*buffer, layout.size);
  n_planes = gdk_memory_format_get_n_planes (layout.format);

  vkCmdPipelineBarrier (state->vk_command_buffer,
                        VK_PIPELINE_STAGE_HOST_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0,
                        0, NULL,
                        1, &(VkBufferMemoryBarrier) {
                            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                            .srcAccessMask = VK_ACCESS_HOST_WRITE_BIT,
                            .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                            .buffer = gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (*buffer)),
                            .offset = 0,
                            .size = VK_WHOLE_SIZE,
                        },
                        0, NULL);
  gsk_vulkan_image_transition (image, 
                               state->semaphores,
                               state->vk_command_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_ACCESS_TRANSFER_WRITE_BIT);

  for (i = 0; i < n_planes; i++)
    {
      gsize block_width = gdk_memory_format_get_plane_block_width (layout.format, i);
      gsize block_height = gdk_memory_format_get_plane_block_height (layout.format, i);
      gsize block_bytes = gdk_memory_format_get_plane_block_bytes (layout.format, i);

      buffer_image_copy[i] = (VkBufferImageCopy) {
                                 .bufferOffset = layout.planes[i].offset,
                                 .bufferRowLength = layout.planes[i].stride / block_bytes,
                                 .bufferImageHeight = layout.height / block_height,
                                 .imageSubresource = {
                                     .aspectMask = n_planes == 1 ? VK_IMAGE_ASPECT_COLOR_BIT : aspect_flags[i],
                                     .mipLevel = 0,
                                     .baseArrayLayer = 0,
                                     .layerCount = 1
                                 },
                                 .imageOffset = {
                                     .x = area->x / block_width,
                                     .y = area->y / block_height,
                                     .z = 0
                                 },
                                 .imageExtent = {
                                     .width = layout.width / block_width,
                                     .height = layout.height / block_height,
                                     .depth = 1
                                 }
                             };
    }

  vkCmdCopyBufferToImage (state->vk_command_buffer,
                          gsk_vulkan_buffer_get_vk_buffer (GSK_VULKAN_BUFFER (*buffer)),
                          gsk_vulkan_image_get_vk_image (image),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          n_planes,
                          buffer_image_copy);

  return op->next;
}

static GskGpuOp *
gsk_gpu_upload_op_vk_command (GskGpuOp              *op,
                              GskGpuFrame           *frame,
                              GskVulkanCommandState *state,
                              GskVulkanImage        *image,
                              void                 (* draw_func) (GskGpuOp *, guchar *, const GdkMemoryLayout *),
                              GskGpuBuffer         **buffer)
{
  GdkMemoryLayout layout;
  guchar *data;

  data = gsk_vulkan_image_get_data (image, &layout);
  if (data)
    {
      draw_func (op, data, &layout);

      *buffer = NULL;

      return op->next;
    }

  return gsk_gpu_upload_op_vk_command_with_area (op,
                                                 frame,
                                                 state,
                                                 image,
                                                 &(cairo_rectangle_int_t) {
                                                     0, 0,
                                                     gsk_gpu_image_get_width (GSK_GPU_IMAGE (image)),
                                                     gsk_gpu_image_get_height (GSK_GPU_IMAGE (image)),
                                                 },
                                                 draw_func,
                                                 buffer);

}
#endif

typedef struct _GskGpuUploadTextureOp GskGpuUploadTextureOp;

struct _GskGpuUploadTextureOp
{
  GskGpuOp op;

  GskGpuImage *image;
  GskGpuBuffer *buffer;
  GdkTexture *texture;
  guint lod_level;
  GskScalingFilter lod_filter;
};

static void
gsk_gpu_upload_texture_op_finish (GskGpuOp *op)
{
  GskGpuUploadTextureOp *self = (GskGpuUploadTextureOp *) op;

  g_object_unref (self->image);
  g_clear_object (&self->buffer);
  g_object_unref (self->texture);
}

static void
gsk_gpu_upload_texture_op_print (GskGpuOp    *op,
                                 GskGpuFrame *frame,
                                 GString     *string,
                                 guint        indent)
{
  GskGpuUploadTextureOp *self = (GskGpuUploadTextureOp *) op;

  gsk_gpu_print_op (string, indent, "upload-texture");
  gsk_gpu_print_image (string, self->image);
  if (self->lod_level > 0)
    g_string_append_printf (string, " @%ux %s",
                            1 << self->lod_level,
                            self->lod_filter == GSK_SCALING_FILTER_TRILINEAR ? "linear" : "nearest");
  gsk_gpu_print_newline (string);
}

static void
gsk_gpu_upload_texture_op_draw (GskGpuOp              *op,
                                guchar                *data,
                                const GdkMemoryLayout *layout)
{
  GskGpuUploadTextureOp *self = (GskGpuUploadTextureOp *) op;

  if (self->lod_level == 0)
    {
      gdk_texture_do_download (self->texture, data, layout, gdk_texture_get_color_state (self->texture));
    }
  else
    {
      GdkMemoryLayout bytes_layout;
      GBytes *bytes;
      
      bytes = gdk_texture_download_bytes (self->texture, &bytes_layout);
      gdk_memory_mipmap (data,
                         layout,
                         g_bytes_get_data (bytes, NULL),
                         &bytes_layout,
                         self->lod_level,
                         self->lod_filter == GSK_SCALING_FILTER_TRILINEAR ? TRUE : FALSE);
      g_bytes_unref (bytes);
    }
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_upload_texture_op_vk_command (GskGpuOp              *op,
                                      GskGpuFrame           *frame,
                                      GskVulkanCommandState *state)
{
  GskGpuUploadTextureOp *self = (GskGpuUploadTextureOp *) op;

  return gsk_gpu_upload_op_vk_command (op,
                                       frame,
                                       state,
                                       GSK_VULKAN_IMAGE (self->image),
                                       gsk_gpu_upload_texture_op_draw,
                                       &self->buffer);
}
#endif

static GskGpuOp *
gsk_gpu_upload_texture_op_gl_command (GskGpuOp          *op,
                                      GskGpuFrame       *frame,
                                      GskGLCommandState *state)
{
  GskGpuUploadTextureOp *self = (GskGpuUploadTextureOp *) op;

  return gsk_gpu_upload_op_gl_command (op,
                                       frame,
                                       self->image,
                                       gsk_gpu_upload_texture_op_draw);
}

static const GskGpuOpClass GSK_GPU_UPLOAD_TEXTURE_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuUploadTextureOp),
  GSK_GPU_STAGE_UPLOAD,
  gsk_gpu_upload_texture_op_finish,
  gsk_gpu_upload_texture_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_upload_texture_op_vk_command,
#endif
  gsk_gpu_upload_texture_op_gl_command
};

GskGpuImage *
gsk_gpu_upload_texture_op_try (GskGpuFrame      *frame,
                               gboolean          with_mipmap,
                               guint             lod_level,
                               GskScalingFilter  lod_filter,
                               GdkTexture       *texture)
{
  GskGpuUploadTextureOp *self;
  GskGpuImage *image;
  GdkMemoryFormat format;

  format = gdk_texture_get_format (texture);

  image = gsk_gpu_device_create_upload_image (gsk_gpu_frame_get_device (frame),
                                              with_mipmap,
                                              lod_level == 0 ? format
                                                             : gdk_memory_format_get_mipmap_format (format),
                                              gdk_memory_format_alpha (format) == GDK_MEMORY_ALPHA_PREMULTIPLIED
                                              ? GSK_GPU_CONVERSION_NONE
                                              : gsk_gpu_color_state_get_conversion (gdk_texture_get_color_state (texture)),
                                              (gdk_texture_get_width (texture) + (1 << lod_level) - 1) >> lod_level,
                                              (gdk_texture_get_height (texture) + (1 << lod_level) - 1) >> lod_level);
  if (image == NULL)
    return NULL;

  if (GSK_DEBUG_CHECK (FALLBACK))
    {
      GEnumClass *enum_class = g_type_class_ref (GDK_TYPE_MEMORY_FORMAT);

      if (!GDK_IS_MEMORY_TEXTURE (texture))
        {
          if (GDK_IS_DMABUF_TEXTURE (texture))
            {
              const GdkDmabuf *dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));
              gdk_debug_message ("Unoptimized upload for dmabuf %.4s:%#" G_GINT64_MODIFIER "x",
                                 (char *) &dmabuf->fourcc, dmabuf->modifier);
            }
          else
            {
              gdk_debug_message ("Unoptimized upload for %s", G_OBJECT_TYPE_NAME (texture));
            }
        }

      if (gdk_texture_get_format (texture) != gsk_gpu_image_get_format (image))
        {
          gdk_debug_message ("Unsupported format %s, converting on CPU to %s",
                             g_enum_get_value (enum_class, gdk_texture_get_format (texture))->value_nick,
                             g_enum_get_value (enum_class, gsk_gpu_image_get_format (image))->value_nick);
        }
      if (with_mipmap && !(gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_CAN_MIPMAP))
        {
          gdk_debug_message ("Format %s does not support mipmaps",
                             g_enum_get_value (enum_class, gsk_gpu_image_get_format (image))->value_nick);
        }

      g_type_class_unref (enum_class);
    }

  self = (GskGpuUploadTextureOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_UPLOAD_TEXTURE_OP_CLASS);

  self->texture = g_object_ref (texture);
  self->lod_level = lod_level;
  self->lod_filter = lod_filter;
  self->image = image;

  return g_object_ref (self->image);
}

typedef struct _GskGpuUploadCairoOp GskGpuUploadCairoOp;

struct _GskGpuUploadCairoOp
{
  GskGpuOp op;

  GskGpuImage *image;
  cairo_rectangle_int_t area;
  graphene_rect_t viewport;
  GskGpuCairoFunc func;
  GskGpuCairoPrintFunc print_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GskGpuBuffer *buffer;
};

static void
gsk_gpu_upload_cairo_op_finish (GskGpuOp *op)
{
  GskGpuUploadCairoOp *self = (GskGpuUploadCairoOp *) op;

  g_object_unref (self->image);
  if (self->user_destroy)
    self->user_destroy (self->user_data);
  g_clear_object (&self->buffer);
}

static void
gsk_gpu_upload_cairo_op_print (GskGpuOp    *op,
                               GskGpuFrame *frame,
                               GString     *string,
                               guint        indent)
{
  GskGpuUploadCairoOp *self = (GskGpuUploadCairoOp *) op;

  gsk_gpu_print_op (string, indent, "upload-cairo");
  gsk_gpu_print_int_rect (string, &self->area);
  gsk_gpu_print_image (string, self->image);
  if (self->print_func)
    self->print_func (self->user_data, string);
  gsk_gpu_print_newline (string);
}

static void
gsk_gpu_upload_cairo_op_draw (GskGpuOp              *op,
                              guchar                *data,
                              const GdkMemoryLayout *layout)
{
  GskGpuUploadCairoOp *self = (GskGpuUploadCairoOp *) op;
  cairo_surface_t *surface;
  float sx, sy;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 self->area.width,
                                                 self->area.height,
                                                 layout->planes[0].stride);
  sx = self->area.width / self->viewport.size.width;
  sy = self->area.height / self->viewport.size.height;
  cairo_surface_set_device_scale (surface, sx, sy);
  cairo_surface_set_device_offset (surface, - sx * self->viewport.origin.x,
                                            - sy * self->viewport.origin.y);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  self->func (self->user_data, cr);

  cairo_destroy (cr);

  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

#ifdef GDK_RENDERING_VULKAN
static GskGpuOp *
gsk_gpu_upload_cairo_op_vk_command (GskGpuOp              *op,
                                    GskGpuFrame           *frame,
                                    GskVulkanCommandState *state)
{
  GskGpuUploadCairoOp *self = (GskGpuUploadCairoOp *) op;

  return gsk_gpu_upload_op_vk_command_with_area (op,
                                                 frame,
                                                 state,
                                                 GSK_VULKAN_IMAGE (self->image),
                                                 &self->area,
                                                 gsk_gpu_upload_cairo_op_draw,
                                                 &self->buffer);
}
#endif

static GskGpuOp *
gsk_gpu_upload_cairo_op_gl_command (GskGpuOp          *op,
                                    GskGpuFrame       *frame,
                                    GskGLCommandState *state)
{
  GskGpuUploadCairoOp *self = (GskGpuUploadCairoOp *) op;

  return gsk_gpu_upload_op_gl_command_with_area (op,
                                                 frame,
                                                 self->image,
                                                 &self->area,
                                                 gsk_gpu_upload_cairo_op_draw);
}

static const GskGpuOpClass GSK_GPU_UPLOAD_CAIRO_OP_CLASS = {
  GSK_GPU_OP_SIZE (GskGpuUploadCairoOp),
  GSK_GPU_STAGE_UPLOAD,
  gsk_gpu_upload_cairo_op_finish,
  gsk_gpu_upload_cairo_op_print,
#ifdef GDK_RENDERING_VULKAN
  gsk_gpu_upload_cairo_op_vk_command,
#endif
  gsk_gpu_upload_cairo_op_gl_command
};

GskGpuImage *
gsk_gpu_upload_cairo_op (GskGpuFrame           *frame,
                         const graphene_vec2_t *scale,
                         const graphene_rect_t *viewport,
                         GskGpuCairoFunc        func,
                         gpointer               user_data,
                         GDestroyNotify         user_destroy)
{
  GskGpuImage *image;

  image = gsk_gpu_device_create_upload_image (gsk_gpu_frame_get_device (frame),
                                              FALSE,
                                              GDK_MEMORY_DEFAULT,
                                              gsk_gpu_color_state_get_conversion (GDK_COLOR_STATE_SRGB),
                                              ceil (graphene_vec2_get_x (scale) * viewport->size.width),
                                              ceil (graphene_vec2_get_y (scale) * viewport->size.height));
  g_assert (image != NULL);

  gsk_gpu_upload_cairo_into_op (frame,
                                image,
                                &(const cairo_rectangle_int_t) {
                                  0, 0,
                                  gsk_gpu_image_get_width (image),
                                  gsk_gpu_image_get_height (image),
                                },
                                viewport,
                                func,
                                NULL,
                                user_data,
                                user_destroy);

  g_object_unref (image);

  return image;
}

void
gsk_gpu_upload_cairo_into_op (GskGpuFrame                 *frame,
                              GskGpuImage                 *image,
                              const cairo_rectangle_int_t *area,
                              const graphene_rect_t       *viewport,
                              GskGpuCairoFunc              func,
                              GskGpuCairoPrintFunc         print_func,
                              gpointer                     user_data,
                              GDestroyNotify               user_destroy)
{
  GskGpuUploadCairoOp *self;

  self = (GskGpuUploadCairoOp *) gsk_gpu_op_alloc (frame, &GSK_GPU_UPLOAD_CAIRO_OP_CLASS);

  self->image = g_object_ref (image);
  self->area = *area;
  self->viewport = *viewport;
  self->func = func;
  self->print_func = print_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;
}
