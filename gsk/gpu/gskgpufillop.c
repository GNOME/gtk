#include "config.h"

#include "gskgpufillopprivate.h"

#include "gskgpuframeprivate.h"
#include "gskgpuprintprivate.h"
#include "gskrectprivate.h"
#include "gskpathprivate.h"
#include "gskcontourprivate.h"

#include "gdk/gdkrgbaprivate.h"

#include "gpu/shaders/gskgpufillinstance.h"

typedef struct _GskGpuFillOp GskGpuFillOp;

struct _GskGpuFillOp
{
  GskGpuShaderOp op;
};

static void
gsk_gpu_fill_op_print_instance (GskGpuShaderOp *shader,
                                gpointer        instance_,
                                GString        *string)
{
  GskGpuFillInstance *instance = (GskGpuFillInstance *) instance_;

  gsk_gpu_print_rect (string, instance->rect);
  gsk_gpu_print_rgba (string, instance->color);
}

static const GskGpuShaderOpClass GSK_GPU_FILL_OP_CLASS = {
  {
    GSK_GPU_OP_SIZE (GskGpuFillOp),
    GSK_GPU_STAGE_SHADER,
    gsk_gpu_shader_op_finish,
    gsk_gpu_shader_op_print,
#ifdef GDK_RENDERING_VULKAN
    gsk_gpu_shader_op_vk_command,
#endif
    gsk_gpu_shader_op_gl_command
  },
  "gskgpufill",
  gsk_gpu_fill_n_textures,
  sizeof (GskGpuFillInstance),
#ifdef GDK_RENDERING_VULKAN
  &gsk_gpu_fill_info,
#endif
  gsk_gpu_fill_op_print_instance,
  gsk_gpu_fill_setup_attrib_locations,
  gsk_gpu_fill_setup_vao
};

static void
write_path_data (GskGpuFrame *frame,
                 GskPath     *path,
                 guint32     *id)
{
  gsize size;
  gsize n;
  guchar *data, *p;
  gsize offset;

  size = sizeof (guint32);
  n = gsk_path_get_n_contours (path);
  for (gsize i = 0; i < n; i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);
      size += gsk_contour_get_shader_size (contour);
    }

  data = p = g_new (guchar, size);

  *(guint32 *) p = n;
  p += sizeof (guint32);

  for (gsize i = 0; i < n; i++)
    {
      const GskContour *contour = gsk_path_get_contour (path, i);
      gsk_contour_to_shader (contour, p);
      p += gsk_contour_get_shader_size (contour);
    }

  gsk_gpu_frame_write_storage_buffer (frame, data, size, &offset);
  *id = (guint) offset;

  g_free (data);
}

void
gsk_gpu_fill_op (GskGpuFrame            *frame,
                 GskGpuShaderClip        clip,
                 GdkColorState          *ccs,
                 float                   opacity,
                 const graphene_point_t *offset,
                 const graphene_rect_t  *rect,
                 GskPath                *path,
                 GskFillRule             fill_rule,
                 const GdkColor         *color)

{
  GskGpuFillInstance *instance;
  GdkColorState *alt;

  alt = gsk_gpu_color_states_find (ccs, color);

  gsk_gpu_shader_op_alloc (frame,
                           &GSK_GPU_FILL_OP_CLASS,
                           gsk_gpu_color_states_create_equal (TRUE, TRUE),
                           fill_rule,
                           clip,
                           NULL,
                           NULL,
                           &instance);

  gsk_gpu_rect_to_float (rect, offset, instance->rect);
  gsk_gpu_color_to_float (color, alt, opacity, instance->color);
  write_path_data (frame, path, &instance->points_id);
}
