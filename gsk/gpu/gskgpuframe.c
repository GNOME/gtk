#include "config.h"

#include "gskgpuframeprivate.h"

#include "gskgpubufferprivate.h"
#include "gskgpucacheprivate.h"
#include "gskgpudeviceprivate.h"
#include "gskgpudownloadopprivate.h"
#include "gskgpuglobalsopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpunodeprocessorprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpurendererprivate.h"
#include "gskgpuuploadopprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"

#include "gdk/gdkdmabufdownloaderprivate.h"
#include "gdk/gdkdmabuftextureprivate.h"
#include "gdk/gdkdrawcontextprivate.h"
#include "gdk/gdktexturedownloaderprivate.h"

#define DEFAULT_VERTEX_BUFFER_SIZE 128 * 1024

/* GL_MAX_UNIFORM_BLOCK_SIZE is at 16384 */
#define DEFAULT_STORAGE_BUFFER_SIZE 16 * 1024 * 64

#define DEFAULT_N_GLOBALS (16384 / sizeof (GskGpuGlobalsInstance))

#define GDK_ARRAY_NAME gsk_gpu_ops
#define GDK_ARRAY_TYPE_NAME GskGpuOps
#define GDK_ARRAY_ELEMENT_TYPE guchar
#define GDK_ARRAY_BY_VALUE 1
#include "gdk/gdkarrayimpl.c"

typedef struct _GskGpuFramePrivate GskGpuFramePrivate;

struct _GskGpuFramePrivate
{
  GskGpuRenderer *renderer;
  GskGpuDevice *device;
  GskGpuOptimizations optimizations;
  gsize texture_vertex_size;
  gint64 timestamp;

  GskGpuOps ops;
  GskGpuOp *first_op;
  GskGpuOp *last_op;

  GskGpuBuffer *vertex_buffer;
  guchar *vertex_buffer_data;
  gsize vertex_buffer_used;
  GskGpuBuffer *globals_buffer;
  GskGpuGlobalsInstance *globals_buffer_data;
  gsize n_globals;
  GskGpuBuffer *storage_buffer;
  guchar *storage_buffer_data;
  gsize storage_buffer_used;
};

G_DEFINE_TYPE_WITH_PRIVATE (GskGpuFrame, gsk_gpu_frame, G_TYPE_OBJECT)

static void
gsk_gpu_frame_default_setup (GskGpuFrame *self)
{
}

static void
gsk_gpu_frame_default_cleanup (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  GskGpuOp *op;
  gsize i;

  priv->n_globals = 0;

  for (i = 0; i < gsk_gpu_ops_get_size (&priv->ops); i += op->op_class->size)
    {
      op = (GskGpuOp *) gsk_gpu_ops_index (&priv->ops, i);

      gsk_gpu_op_finish (op);
    }
  gsk_gpu_ops_set_size (&priv->ops, 0);

  priv->first_op = NULL;
  priv->last_op = NULL;
}

static void
gsk_gpu_frame_default_begin (GskGpuFrame           *self,
                             GdkDrawContext        *context,
                             GdkMemoryDepth         depth,
                             const cairo_region_t  *region,
                             const graphene_rect_t *opaque)
{
  gdk_draw_context_begin_frame_full (context, depth, region, opaque);
}

static void
gsk_gpu_frame_default_end (GskGpuFrame    *self,
                           GdkDrawContext *context)
{
  gdk_draw_context_end_frame_full (context);
}

static gboolean
gsk_gpu_frame_is_clean (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return gsk_gpu_ops_get_size (&priv->ops) == 0;
}

static void
gsk_gpu_frame_cleanup (GskGpuFrame *self)
{
  if (gsk_gpu_frame_is_clean (self))
    return;

  GSK_GPU_FRAME_GET_CLASS (self)->cleanup (self);
}

static GskGpuImage *
gsk_gpu_frame_default_upload_texture (GskGpuFrame *self,
                                      gboolean     with_mipmap,
                                      GdkTexture  *texture)
{
  return NULL;
}

static void
gsk_gpu_frame_dispose (GObject *object)
{
  GskGpuFrame *self = GSK_GPU_FRAME (object);

  gsk_gpu_frame_cleanup (self);

  G_OBJECT_CLASS (gsk_gpu_frame_parent_class)->dispose (object);
}

static void
gsk_gpu_frame_finalize (GObject *object)
{
  GskGpuFrame *self = GSK_GPU_FRAME (object);
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  gsk_gpu_ops_clear (&priv->ops);

  g_clear_object (&priv->vertex_buffer);
  g_clear_object (&priv->globals_buffer);
  g_clear_object (&priv->storage_buffer);

  g_object_unref (priv->device);

  G_OBJECT_CLASS (gsk_gpu_frame_parent_class)->finalize (object);
}

static void
gsk_gpu_frame_class_init (GskGpuFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->setup = gsk_gpu_frame_default_setup;
  klass->cleanup = gsk_gpu_frame_default_cleanup;
  klass->begin = gsk_gpu_frame_default_begin;
  klass->end = gsk_gpu_frame_default_end;
  klass->upload_texture = gsk_gpu_frame_default_upload_texture;

  object_class->dispose = gsk_gpu_frame_dispose;
  object_class->finalize = gsk_gpu_frame_finalize;
}

static void
gsk_gpu_frame_init (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  gsk_gpu_ops_init (&priv->ops);
}

void
gsk_gpu_frame_setup (GskGpuFrame         *self,
                     GskGpuRenderer      *renderer,
                     GskGpuDevice        *device,
                     GskGpuOptimizations  optimizations)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  /* no reference, the renderer owns us */
  priv->renderer = renderer;
  priv->device = g_object_ref (device);
  priv->optimizations = optimizations;

  GSK_GPU_FRAME_GET_CLASS (self)->setup (self);
}

/*
 * gsk_gpu_frame_set_texture_vertex_size:
 * @self: the frame
 * @texture_vertex_size: bytes to reserve in the vertex data per
 *   texture rendered
 *
 * Some renderers want to attach vertex data for textures, usually
 * for supporting bindless textures. This is the number of bytes
 * reserved per texture.
 *
 * GskGpuFrameClass::write_texture_vertex_data() is used to write that
 * data.
 **/
void
gsk_gpu_frame_set_texture_vertex_size (GskGpuFrame *self,
                                       gsize        texture_vertex_size)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  priv->texture_vertex_size = texture_vertex_size;
}

void
gsk_gpu_frame_begin (GskGpuFrame          *self,
                     GdkDrawContext       *context,
                     GdkMemoryDepth        depth,
                     const cairo_region_t *region,
                     const graphene_rect_t *opaque)
{
  GSK_GPU_FRAME_GET_CLASS (self)->begin (self, context, depth, region, opaque);
}

void
gsk_gpu_frame_end (GskGpuFrame    *self,
                   GdkDrawContext *context)
{
  GSK_GPU_FRAME_GET_CLASS (self)->end (self, context);
}

GskGpuDevice *
gsk_gpu_frame_get_device (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return priv->device;
}

GdkDrawContext *
gsk_gpu_frame_get_context (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return gsk_gpu_renderer_get_context (priv->renderer);
}

gint64
gsk_gpu_frame_get_timestamp (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return priv->timestamp;
}

gboolean
gsk_gpu_frame_should_optimize (GskGpuFrame         *self,
                               GskGpuOptimizations  optimization)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return (priv->optimizations & optimization) == optimization;
}

static void
gsk_gpu_frame_verbose_print (GskGpuFrame *self,
                             const char  *heading)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  if (GSK_RENDERER_DEBUG_CHECK (GSK_RENDERER (priv->renderer), VERBOSE))
    {
      GskGpuOp *op;
      guint indent = 1;
      GString *string = g_string_new (heading);
      g_string_append (string, ":\n");

      for (op = priv->first_op; op; op = op->next)
        {
          if (op->op_class->stage == GSK_GPU_STAGE_END_PASS)
            indent--;
          gsk_gpu_op_print (op, self, string, indent);
          if (op->op_class->stage == GSK_GPU_STAGE_BEGIN_PASS)
            indent++;
        }

      gdk_debug_message ("%s", string->str);
      g_string_free (string, TRUE);
    }
}

static void
gsk_gpu_frame_seal_ops (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  GskGpuOp *last, *op;
  gsize i;

  if (gsk_gpu_ops_get_size (&priv->ops) == 0)
    return;

  priv->first_op = (GskGpuOp *) gsk_gpu_ops_index (&priv->ops, 0);

  last = priv->first_op;
  for (i = last->op_class->size; i < gsk_gpu_ops_get_size (&priv->ops); i += op->op_class->size)
    {
      op = (GskGpuOp *) gsk_gpu_ops_index (&priv->ops, i);

      last->next = op;
      last = op;
    }
}

typedef struct 
{
  struct {
    GskGpuOp *first;
    GskGpuOp *last;
  } upload, command;
} SortData;

static GskGpuOp *
gsk_gpu_frame_sort_render_pass (GskGpuFrame *self,
                                GskGpuOp    *op,
                                SortData    *sort_data)
{
  SortData subpasses = { { NULL, NULL }, { NULL, NULL } };
  SortData pass = { { NULL, NULL }, { NULL, NULL } };

  if (op->op_class->stage == GSK_GPU_STAGE_BEGIN_PASS)
    {
      pass.command.first = op;
      pass.command.last = op;
      op = op->next;
    }

  while (op)
    {
      switch (op->op_class->stage)
      {
        case GSK_GPU_STAGE_UPLOAD:
          if (pass.upload.first == NULL)
            pass.upload.first = op;
          else
            pass.upload.last->next = op;
          pass.upload.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_COMMAND:
        case GSK_GPU_STAGE_SHADER:
          if (pass.command.first == NULL)
            pass.command.first = op;
          else
            pass.command.last->next = op;
          pass.command.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_PASS:
          if (subpasses.command.first == NULL)
            subpasses.command.first = op;
          else
            subpasses.command.last->next = op;
          subpasses.command.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_BEGIN_PASS:
          /* append subpass to existing subpasses */
          op = gsk_gpu_frame_sort_render_pass (self, op, &subpasses);
          break;

        case GSK_GPU_STAGE_END_PASS:
          pass.command.last->next = op;
          pass.command.last = op;
          op = op->next;
          goto out;

        default:
          g_assert_not_reached ();
          break;
      }
    }

out:
  /* append to the sort data, first the subpasses, then the current pass */
  if (subpasses.upload.first)
    {
      if (sort_data->upload.first != NULL)
        sort_data->upload.last->next = subpasses.upload.first;
      else
        sort_data->upload.first = subpasses.upload.first;
      sort_data->upload.last = subpasses.upload.last;
    }
  if (pass.upload.first)
    {
      if (sort_data->upload.first != NULL)
        sort_data->upload.last->next = pass.upload.first;
      else
        sort_data->upload.first = pass.upload.first;
      sort_data->upload.last = pass.upload.last;
    }
  if (subpasses.command.first)
    {
      if (sort_data->command.first != NULL)
        sort_data->command.last->next = subpasses.command.first;
      else
        sort_data->command.first = subpasses.command.first;
      sort_data->command.last = subpasses.command.last;
    }
  if (pass.command.first)
    {
      if (sort_data->command.first != NULL)
        sort_data->command.last->next = pass.command.first;
      else
        sort_data->command.first = pass.command.first;
      sort_data->command.last = pass.command.last;
    }

  return op;
}

static void
gsk_gpu_frame_sort_ops (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  SortData sort_data = { { NULL, }, };
  GskGpuOp *op;

  op = priv->first_op;
  while (op)
    {
      op = gsk_gpu_frame_sort_render_pass (self, op, &sort_data);
    }

  if (sort_data.upload.first)
    {
      sort_data.upload.last->next = sort_data.command.first;
      priv->first_op = sort_data.upload.first;
    }
  else
    priv->first_op = sort_data.command.first;
  if (sort_data.command.last)
    sort_data.command.last->next = NULL;

  priv->last_op = NULL;
}

gpointer
gsk_gpu_frame_alloc_op (GskGpuFrame *self,
                        gsize        size)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  gsize pos;

  pos = gsk_gpu_ops_get_size (&priv->ops);

  gsk_gpu_ops_splice (&priv->ops,
                      pos,
                      0, FALSE,
                      NULL,
                      size);

  priv->last_op = (GskGpuOp *) gsk_gpu_ops_index (&priv->ops, pos);

  return priv->last_op;
}

GskGpuOp *
gsk_gpu_frame_get_last_op (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return priv->last_op;
}

static GskGpuImage *
gsk_gpu_frame_do_upload_texture (GskGpuFrame  *self,
                                 gboolean      dmabuf_import,
                                 gboolean      with_mipmap,
                                 GdkTexture   *texture)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  GskGpuImage *image;

  image = GSK_GPU_FRAME_GET_CLASS (self)->upload_texture (self, with_mipmap, texture);

  if (image == NULL && !dmabuf_import)
    image = gsk_gpu_upload_texture_op_try (self, with_mipmap, 0, GSK_SCALING_FILTER_NEAREST, texture);

  if (image)
    gsk_gpu_cache_cache_texture_image (gsk_gpu_device_get_cache (priv->device), texture, image, NULL);

  return image;
}

GskGpuImage *
gsk_gpu_frame_upload_texture (GskGpuFrame  *self,
                              gboolean      with_mipmap,
                              GdkTexture   *texture)
{
  return gsk_gpu_frame_do_upload_texture (self, FALSE, with_mipmap, texture);
}

static GskGpuBuffer *
gsk_gpu_frame_create_vertex_buffer (GskGpuFrame *self,
                                    gsize        size)
{
  return GSK_GPU_FRAME_GET_CLASS (self)->create_vertex_buffer (self, size);
}

static GskGpuBuffer *
gsk_gpu_frame_create_globals_buffer (GskGpuFrame *self,
                                     gsize        size)
{
  return GSK_GPU_FRAME_GET_CLASS (self)->create_globals_buffer (self, size);
}

static GskGpuBuffer *
gsk_gpu_frame_create_storage_buffer (GskGpuFrame *self,
                                     gsize        size)
{
  return GSK_GPU_FRAME_GET_CLASS (self)->create_storage_buffer (self, size);
}

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

gsize
gsk_gpu_frame_get_texture_vertex_size (GskGpuFrame *self,
                                       gsize        n_textures)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  return priv->texture_vertex_size * n_textures;
}

gsize
gsk_gpu_frame_reserve_vertex_data (GskGpuFrame *self,
                                   gsize        size)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  gsize size_needed;

  if (priv->vertex_buffer == NULL)
    priv->vertex_buffer = gsk_gpu_frame_create_vertex_buffer (self, DEFAULT_VERTEX_BUFFER_SIZE);

  size_needed = round_up (priv->vertex_buffer_used, size) + size;

  if (gsk_gpu_buffer_get_size (priv->vertex_buffer) < size_needed)
    {
      gsize old_size = gsk_gpu_buffer_get_size (priv->vertex_buffer);
      GskGpuBuffer *new_buffer = gsk_gpu_frame_create_vertex_buffer (self, old_size * 2);
      guchar *new_data = gsk_gpu_buffer_map (new_buffer);

      if (priv->vertex_buffer_data)
        {
          memcpy (new_data, priv->vertex_buffer_data, old_size);
          gsk_gpu_buffer_unmap (priv->vertex_buffer, old_size);
        }
      g_object_unref (priv->vertex_buffer);
      priv->vertex_buffer = new_buffer;
      priv->vertex_buffer_data = new_data;
    }

  priv->vertex_buffer_used = size_needed;

  return size_needed - size;
}

gsize
gsk_gpu_frame_add_globals (GskGpuFrame                 *self,
                           const GskGpuGlobalsInstance *globals)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  gsize size_needed, result;

  if (priv->globals_buffer == NULL)
    {
      priv->globals_buffer = gsk_gpu_frame_create_globals_buffer (self, sizeof (GskGpuGlobalsInstance) * DEFAULT_N_GLOBALS);
      if (priv->globals_buffer == NULL)
        return 0;
    }
  if (priv->globals_buffer_data == NULL)
    priv->globals_buffer_data = (GskGpuGlobalsInstance *) gsk_gpu_buffer_map (priv->globals_buffer);

  size_needed = sizeof (GskGpuGlobalsInstance) * (priv->n_globals + 1);

  if (gsk_gpu_buffer_get_size (priv->globals_buffer) < size_needed)
    {
      gsize old_size = gsk_gpu_buffer_get_size (priv->globals_buffer);
      GskGpuBuffer *new_buffer = gsk_gpu_frame_create_globals_buffer (self, old_size * 2);
      GskGpuGlobalsInstance *new_data = (GskGpuGlobalsInstance *) gsk_gpu_buffer_map (new_buffer);

      if (priv->globals_buffer_data)
        {
          memcpy (new_data, priv->globals_buffer_data, old_size);
          gsk_gpu_buffer_unmap (priv->globals_buffer, old_size);
        }
      g_object_unref (priv->globals_buffer);
      priv->globals_buffer = new_buffer;
      priv->globals_buffer_data = new_data;
    }

  result = priv->n_globals;

  priv->globals_buffer_data[priv->n_globals] = *globals;
  priv->n_globals++;

  return result;
}

guchar *
gsk_gpu_frame_get_vertex_data (GskGpuFrame *self,
                               gsize        offset)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  if (priv->vertex_buffer_data == NULL)
    priv->vertex_buffer_data = gsk_gpu_buffer_map (priv->vertex_buffer);

  return priv->vertex_buffer_data + offset;
}

static void
gsk_gpu_frame_ensure_storage_buffer (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  if (priv->storage_buffer_data != NULL)
    return;

  if (priv->storage_buffer == NULL)
    priv->storage_buffer = gsk_gpu_frame_create_storage_buffer (self, DEFAULT_STORAGE_BUFFER_SIZE);

  priv->storage_buffer_data = gsk_gpu_buffer_map (priv->storage_buffer);
}

void
gsk_gpu_frame_write_texture_vertex_data (GskGpuFrame    *self,
                                         guchar         *data,
                                         GskGpuImage   **images,
                                         GskGpuSampler  *samplers,
                                         gsize           n_images)
{
  GSK_GPU_FRAME_GET_CLASS (self)->write_texture_vertex_data (self, data, images, samplers, n_images);
}

GskGpuBuffer *
gsk_gpu_frame_write_storage_buffer (GskGpuFrame  *self,
                                    const guchar *data,
                                    gsize         size,
                                    gsize        *out_offset)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  gsize offset;

  gsk_gpu_frame_ensure_storage_buffer (self);

  offset = priv->storage_buffer_used;
  if (offset + size > gsk_gpu_buffer_get_size (priv->storage_buffer))
    {
      g_assert (offset > 0);

      gsk_gpu_buffer_unmap (priv->storage_buffer, 0);
      g_clear_object (&priv->storage_buffer);
      priv->storage_buffer_data = 0;
      priv->storage_buffer_used = 0;
      gsk_gpu_frame_ensure_storage_buffer (self);

      offset = priv->storage_buffer_used;
    }

  if (size)
    {
      memcpy (priv->storage_buffer_data + offset, data, size);
      priv->storage_buffer_used += size;
    }

  *out_offset = offset;
  return priv->storage_buffer;
}

gboolean
gsk_gpu_frame_is_busy (GskGpuFrame *self)
{
  if (gsk_gpu_frame_is_clean (self))
    return FALSE;

  return GSK_GPU_FRAME_GET_CLASS (self)->is_busy (self);
}

void
gsk_gpu_frame_wait (GskGpuFrame *self)
{
  if (gsk_gpu_frame_is_clean (self))
    return;

  GSK_GPU_FRAME_GET_CLASS (self)->wait (self);

  gsk_gpu_frame_cleanup (self);
}

static void
gsk_gpu_frame_record (GskGpuFrame            *self,
                      gint64                  timestamp,
                      GskGpuImage            *target,
                      GdkColorState          *target_color_state,
                      cairo_region_t         *clip,
                      GskRenderNode          *node,
                      const graphene_rect_t  *viewport,
                      GdkTexture            **texture)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  GskRenderPassType pass_type = texture ? GSK_RENDER_PASS_EXPORT : GSK_RENDER_PASS_PRESENT;

  priv->timestamp = timestamp;
  gsk_gpu_cache_set_time (gsk_gpu_device_get_cache (priv->device), timestamp);

  gsk_gpu_node_processor_process (self, target, target_color_state, clip, node, viewport, pass_type);

  if (texture)
    gsk_gpu_download_op (self, target, target_color_state, texture);
}

static void
gsk_gpu_frame_submit (GskGpuFrame       *self,
                      GskRenderPassType  pass_type)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  gsk_gpu_frame_seal_ops (self);
  gsk_gpu_frame_verbose_print (self, "start of frame");
  gsk_gpu_frame_sort_ops (self);
  gsk_gpu_frame_verbose_print (self, "after sort");

  if (priv->vertex_buffer)
    {
      gsk_gpu_buffer_unmap (priv->vertex_buffer, priv->vertex_buffer_used);
      priv->vertex_buffer_data = NULL;
      priv->vertex_buffer_used = 0;
    }

  if (priv->globals_buffer)
    {
      gsk_gpu_buffer_unmap (priv->globals_buffer, sizeof (GskGpuGlobalsInstance) * priv->n_globals);
      priv->globals_buffer_data = NULL;
    }

  if (priv->storage_buffer_data)
    {
      gsk_gpu_buffer_unmap (priv->storage_buffer, priv->storage_buffer_used);
      priv->storage_buffer_data = NULL;
      priv->storage_buffer_used = 0;
    }

  GSK_GPU_FRAME_GET_CLASS (self)->submit (self,
                                          pass_type,
                                          priv->vertex_buffer,
                                          priv->globals_buffer,
                                          priv->first_op);
}

void
gsk_gpu_frame_render (GskGpuFrame            *self,
                      gint64                  timestamp,
                      GskGpuImage            *target,
                      GdkColorState          *target_color_state,
                      cairo_region_t         *clip,
                      GskRenderNode          *node,
                      const graphene_rect_t  *viewport,
                      GdkTexture            **texture)
{
  GskRenderPassType pass_type = texture ? GSK_RENDER_PASS_EXPORT : GSK_RENDER_PASS_PRESENT;

  gsk_gpu_frame_cleanup (self);

  gsk_gpu_frame_record (self, timestamp, target, target_color_state, clip, node, viewport, texture);

  gsk_gpu_frame_submit (self, pass_type);
}

static gboolean
image_is_uploaded (GskGpuImage *image)
{
  /* If we explicitly uploaded an image, we don't need the toggle ref to
   * keep the texture alive, because uploaded images are copies. */
  return (gsk_gpu_image_get_flags (image) & GSK_GPU_IMAGE_TOGGLE_REF) == 0;
}

gboolean
gsk_gpu_frame_download_texture (GskGpuFrame     *self,
                                gint64           timestamp,
                                GdkTexture      *texture,
                                GdkMemoryFormat  format,
                                GdkColorState   *color_state,
                                guchar          *data,
                                gsize            stride)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  const GdkDmabuf *dmabuf;
  GdkColorState *image_cs;
  GskGpuImage *image;

  priv->timestamp = timestamp;
  gsk_gpu_cache_set_time (gsk_gpu_device_get_cache (priv->device), timestamp);

  image = gsk_gpu_cache_lookup_texture_image (gsk_gpu_device_get_cache (priv->device), texture, NULL);
  if (image && image_is_uploaded (image))
    image = NULL;

  if (image == NULL)
    image = gsk_gpu_frame_do_upload_texture (self, TRUE, FALSE, texture);

  if (image == NULL)
    return FALSE;

  image_cs = gdk_texture_get_color_state (texture);
  dmabuf = gdk_dmabuf_texture_get_dmabuf (GDK_DMABUF_TEXTURE (texture));

  gsk_gpu_frame_cleanup (self);

  if (gdk_memory_format_get_dmabuf_fourcc (gsk_gpu_image_get_format (image)) != dmabuf->fourcc ||
      image_cs != color_state)
    {
      GskGpuImage *converted;

      converted = gsk_gpu_node_processor_convert_image (self,
                                                        format,
                                                        color_state,
                                                        image,
                                                        image_cs);
      if (converted == NULL)
        {
          g_object_unref (image);
          return FALSE;
        }
      g_object_unref (image);
      image = converted;
      image_cs = color_state;
    }

  gsk_gpu_download_into_op (self,
                            image,
                            image_cs,
                            format,
                            color_state,
                            data,
                            stride);

  gsk_gpu_frame_submit (self, GSK_RENDER_PASS_EXPORT);
  g_object_unref (image);

  return TRUE;
}
