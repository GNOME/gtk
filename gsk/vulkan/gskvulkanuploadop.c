#include "config.h"

#include "gskvulkanuploadopprivate.h"

#include "gskvulkanprivate.h"

#include "gdk/gdkmemoryformatprivate.h"

static gsize
gsk_vulkan_upload_op_count_vertex_data (GskVulkanOp *op,
                                        gsize        n_bytes)
{
  return n_bytes;
}

static void
gsk_vulkan_upload_op_collect_vertex_data (GskVulkanOp *op,
                                          guchar      *data)
{
}

static void
gsk_vulkan_upload_op_reserve_descriptor_sets (GskVulkanOp     *op,
                                              GskVulkanRender *render)
{
}

static GskVulkanOp *
gsk_vulkan_upload_op_command_with_area (GskVulkanOp                 *op,
                                        GskVulkanRender             *render,
                                        VkCommandBuffer              command_buffer,
                                        GskVulkanImage              *image,
                                        const cairo_rectangle_int_t *area,
                                        void           (* draw_func) (GskVulkanOp *, guchar *, gsize),
                                        GskVulkanBuffer            **buffer)
{
  gsize stride;
  guchar *data;

  stride = area->width * gdk_memory_format_bytes_per_pixel (gsk_vulkan_image_get_format (image));
  *buffer = gsk_vulkan_buffer_new_map (gsk_vulkan_render_get_context (render),
                                       area->height * stride,
                                       GSK_VULKAN_WRITE);
  data = gsk_vulkan_buffer_map (*buffer);

  draw_func (op, data, stride);

  gsk_vulkan_buffer_unmap (*buffer);

  vkCmdPipelineBarrier (command_buffer,
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
                            .buffer = gsk_vulkan_buffer_get_buffer (*buffer),
                            .offset = 0,
                            .size = VK_WHOLE_SIZE,
                        },
                        0, NULL);
  gsk_vulkan_image_transition (image, 
                               command_buffer,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_ACCESS_TRANSFER_WRITE_BIT);

  vkCmdCopyBufferToImage (command_buffer,
                          gsk_vulkan_buffer_get_buffer (*buffer),
                          gsk_vulkan_image_get_vk_image (image),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          1,
                          (VkBufferImageCopy[1]) {
                               {
                                   .bufferOffset = 0,
                                   .bufferRowLength = area->width,
                                   .bufferImageHeight = area->height,
                                   .imageSubresource = {
                                       .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                       .mipLevel = 0,
                                       .baseArrayLayer = 0,
                                       .layerCount = 1
                                   },
                                   .imageOffset = {
                                       .x = area->x,
                                       .y = area->y,
                                       .z = 0
                                   },
                                   .imageExtent = {
                                       .width = area->width,
                                       .height = area->height,
                                       .depth = 1
                                   }
                               }
                          });

  return op->next;
}

static GskVulkanOp *
gsk_vulkan_upload_op_command (GskVulkanOp      *op,
                              GskVulkanRender  *render,
                              VkCommandBuffer   command_buffer,
                              GskVulkanImage   *image,
                              void              (* draw_func) (GskVulkanOp *, guchar *, gsize),
                              GskVulkanBuffer **buffer)
{
  gsize stride;
  guchar *data;

  data = gsk_vulkan_image_try_map (image, &stride);
  if (data)
    {
      draw_func (op, data, stride);

      gsk_vulkan_image_unmap (image);
      *buffer = NULL;

      return op->next;
    }

  return gsk_vulkan_upload_op_command_with_area (op,
                                                 render,
                                                 command_buffer,
                                                 image,
                                                 &(cairo_rectangle_int_t) {
                                                     0, 0,
                                                     gsk_vulkan_image_get_width (image),
                                                     gsk_vulkan_image_get_height (image),
                                                 },
                                                 draw_func,
                                                 buffer);

}

typedef struct _GskVulkanUploadTextureOp GskVulkanUploadTextureOp;

struct _GskVulkanUploadTextureOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskVulkanBuffer *buffer;
  GdkTexture *texture;
};

static void
gsk_vulkan_upload_texture_op_finish (GskVulkanOp *op)
{
  GskVulkanUploadTextureOp *self = (GskVulkanUploadTextureOp *) op;

  g_object_unref (self->image);
  g_clear_pointer (&self->buffer, gsk_vulkan_buffer_free);
  g_object_unref (self->texture);
}

static void
gsk_vulkan_upload_texture_op_print (GskVulkanOp *op,
                                    GString     *string,
                                    guint        indent)
{
  GskVulkanUploadTextureOp *self = (GskVulkanUploadTextureOp *) op;

  print_indent (string, indent);
  g_string_append (string, "upload-texture ");
  print_image (string, self->image);
  print_newline (string);
}

static void
gsk_vulkan_upload_texture_op_draw (GskVulkanOp *op,
                                   guchar      *data,
                                   gsize        stride)
{
  GskVulkanUploadTextureOp *self = (GskVulkanUploadTextureOp *) op;
  GdkTextureDownloader *downloader;

  downloader = gdk_texture_downloader_new (self->texture);
  gdk_texture_downloader_set_format (downloader, gsk_vulkan_image_get_format (self->image));
  gdk_texture_downloader_download_into (downloader, data, stride);
  gdk_texture_downloader_free (downloader);
}

static GskVulkanOp *
gsk_vulkan_upload_texture_op_command (GskVulkanOp      *op,
                                      GskVulkanRender  *render,
                                      VkRenderPass      render_pass,
                                      VkCommandBuffer   command_buffer)
{
  GskVulkanUploadTextureOp *self = (GskVulkanUploadTextureOp *) op;

  return gsk_vulkan_upload_op_command (op,
                                       render,
                                       command_buffer,
                                       self->image,
                                       gsk_vulkan_upload_texture_op_draw,
                                       &self->buffer);
}

static const GskVulkanOpClass GSK_VULKAN_UPLOAD_TEXTURE_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanUploadTextureOp),
  GSK_VULKAN_STAGE_UPLOAD,
  gsk_vulkan_upload_texture_op_finish,
  gsk_vulkan_upload_texture_op_print,
  gsk_vulkan_upload_op_count_vertex_data,
  gsk_vulkan_upload_op_collect_vertex_data,
  gsk_vulkan_upload_op_reserve_descriptor_sets,
  gsk_vulkan_upload_texture_op_command
};

GskVulkanImage *
gsk_vulkan_upload_texture_op (GskVulkanRender  *render,
                              GdkTexture       *texture)
{
  GskVulkanUploadTextureOp *self;

  self = (GskVulkanUploadTextureOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_UPLOAD_TEXTURE_OP_CLASS);

  self->texture = g_object_ref (texture);
  self->image = gsk_vulkan_image_new_for_upload (gsk_vulkan_render_get_context (render),
                                                 gdk_texture_get_format (texture),
                                                 gdk_texture_get_width (texture),
                                                 gdk_texture_get_height (texture));

  return self->image;
}

typedef struct _GskVulkanUploadCairoOp GskVulkanUploadCairoOp;

struct _GskVulkanUploadCairoOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  GskRenderNode *node;
  graphene_rect_t viewport;

  GskVulkanBuffer *buffer;
};

static void
gsk_vulkan_upload_cairo_op_finish (GskVulkanOp *op)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;

  g_object_unref (self->image);
  gsk_render_node_unref (self->node);

  g_clear_pointer (&self->buffer, gsk_vulkan_buffer_free);
}

static void
gsk_vulkan_upload_cairo_op_print (GskVulkanOp *op,
                                  GString     *string,
                                  guint        indent)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;

  print_indent (string, indent);
  g_string_append (string, "upload-cairo ");
  print_image (string, self->image);
  print_newline (string);
}

static void
gsk_vulkan_upload_cairo_op_draw (GskVulkanOp *op,
                                 guchar      *data,
                                 gsize        stride)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;
  cairo_surface_t *surface;
  cairo_t *cr;
  int width, height;

  width = gsk_vulkan_image_get_width (self->image);
  height = gsk_vulkan_image_get_height (self->image);

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width, height,
                                                 stride);
  cairo_surface_set_device_scale (surface,
                                  width / self->viewport.size.width,
                                  height / self->viewport.size.height);
  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
  cairo_translate (cr, -self->viewport.origin.x, -self->viewport.origin.y);

  gsk_render_node_draw (self->node, cr);

  cairo_destroy (cr);

  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static GskVulkanOp *
gsk_vulkan_upload_cairo_op_command (GskVulkanOp      *op,
                                    GskVulkanRender  *render,
                                    VkRenderPass      render_pass,
                                    VkCommandBuffer   command_buffer)
{
  GskVulkanUploadCairoOp *self = (GskVulkanUploadCairoOp *) op;

  return gsk_vulkan_upload_op_command (op,
                                       render,
                                       command_buffer,
                                       self->image,
                                       gsk_vulkan_upload_cairo_op_draw,
                                       &self->buffer);
}

static const GskVulkanOpClass GSK_VULKAN_UPLOAD_CAIRO_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanUploadCairoOp),
  GSK_VULKAN_STAGE_UPLOAD,
  gsk_vulkan_upload_cairo_op_finish,
  gsk_vulkan_upload_cairo_op_print,
  gsk_vulkan_upload_op_count_vertex_data,
  gsk_vulkan_upload_op_collect_vertex_data,
  gsk_vulkan_upload_op_reserve_descriptor_sets,
  gsk_vulkan_upload_cairo_op_command
};

GskVulkanImage *
gsk_vulkan_upload_cairo_op (GskVulkanRender       *render,
                            GskRenderNode         *node,
                            const graphene_vec2_t *scale,
                            const graphene_rect_t *viewport)
{
  GskVulkanUploadCairoOp *self;

  self = (GskVulkanUploadCairoOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_UPLOAD_CAIRO_OP_CLASS);

  self->node = gsk_render_node_ref (node);
  self->image = gsk_vulkan_image_new_for_upload (gsk_vulkan_render_get_context (render),
                                                 GDK_MEMORY_DEFAULT,
                                                 ceil (graphene_vec2_get_x (scale) * viewport->size.width),
                                                 ceil (graphene_vec2_get_y (scale) * viewport->size.height));
  g_assert (gsk_vulkan_image_get_postprocess (self->image) == 0);
  self->viewport = *viewport;

  return self->image;
}

typedef struct _GskVulkanUploadGlyphOp GskVulkanUploadGlyphOp;

struct _GskVulkanUploadGlyphOp
{
  GskVulkanOp op;

  GskVulkanImage *image;
  cairo_rectangle_int_t area;
  PangoFont *font;
  PangoGlyphInfo glyph_info;
  float scale;

  GskVulkanBuffer *buffer;
};

static void
gsk_vulkan_upload_glyph_op_finish (GskVulkanOp *op)
{
  GskVulkanUploadGlyphOp *self = (GskVulkanUploadGlyphOp *) op;

  g_object_unref (self->image);
  g_object_unref (self->font);

  g_clear_pointer (&self->buffer, gsk_vulkan_buffer_free);
}

static void
gsk_vulkan_upload_glyph_op_print (GskVulkanOp *op,
                                  GString     *string,
                                  guint        indent)
{
  GskVulkanUploadGlyphOp *self = (GskVulkanUploadGlyphOp *) op;

  print_indent (string, indent);
  g_string_append (string, "upload-glyph ");
  print_int_rect (string, &self->area);
  g_string_append_printf (string, "glyph %u @ %g ", self->glyph_info.glyph, self->scale);
  print_newline (string);
}

static void
gsk_vulkan_upload_glyph_op_draw (GskVulkanOp *op,
                                 guchar      *data,
                                 gsize        stride)
{
  GskVulkanUploadGlyphOp *self = (GskVulkanUploadGlyphOp *) op;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create_for_data (data,
                                                 CAIRO_FORMAT_ARGB32,
                                                 self->area.width,
                                                 self->area.height,
                                                 stride);
  cairo_surface_set_device_scale (surface, self->scale, self->scale);

  cr = cairo_create (surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
  cairo_paint (cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  /* Make sure the entire surface is initialized to black */
  cairo_set_source_rgba (cr, 0, 0, 0, 0);
  cairo_rectangle (cr, 0.0, 0.0, self->area.width, self->area.height);
  cairo_fill (cr);

  /* Draw glyph */
  cairo_set_source_rgba (cr, 1, 1, 1, 1);

  pango_cairo_show_glyph_string (cr,
                                 self->font,
                                 &(PangoGlyphString) {
                                   .num_glyphs = 1,
                                   .glyphs = &self->glyph_info
                                 });

  cairo_destroy (cr);

  cairo_surface_finish (surface);
  cairo_surface_destroy (surface);
}

static GskVulkanOp *
gsk_vulkan_upload_glyph_op_command (GskVulkanOp      *op,
                                    GskVulkanRender  *render,
                                    VkRenderPass      render_pass,
                                    VkCommandBuffer   command_buffer)
{
  GskVulkanUploadGlyphOp *self = (GskVulkanUploadGlyphOp *) op;

  return gsk_vulkan_upload_op_command_with_area (op,
                                                 render,
                                                 command_buffer,
                                                 self->image,
                                                 &self->area,
                                                 gsk_vulkan_upload_glyph_op_draw,
                                                 &self->buffer);
}

static const GskVulkanOpClass GSK_VULKAN_UPLOAD_GLYPH_OP_CLASS = {
  GSK_VULKAN_OP_SIZE (GskVulkanUploadGlyphOp),
  GSK_VULKAN_STAGE_UPLOAD,
  gsk_vulkan_upload_glyph_op_finish,
  gsk_vulkan_upload_glyph_op_print,
  gsk_vulkan_upload_op_count_vertex_data,
  gsk_vulkan_upload_op_collect_vertex_data,
  gsk_vulkan_upload_op_reserve_descriptor_sets,
  gsk_vulkan_upload_glyph_op_command
};

void
gsk_vulkan_upload_glyph_op (GskVulkanRender       *render,
                            GskVulkanImage        *image,
                            cairo_rectangle_int_t *area,
                            PangoFont             *font,
                            PangoGlyphInfo        *glyph_info,
                            float                  scale)
{
  GskVulkanUploadGlyphOp *self;

  self = (GskVulkanUploadGlyphOp *) gsk_vulkan_op_alloc (render, &GSK_VULKAN_UPLOAD_GLYPH_OP_CLASS);

  self->image = g_object_ref (image);
  self->area = *area;
  self->font = g_object_ref (font);
  self->glyph_info = *glyph_info;
  self->scale = scale;
}
