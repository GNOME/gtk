#include "config.h"

#include "gskgpuframeprivate.h"

#include "gskgpudeviceprivate.h"
#include "gskgpudownloadopprivate.h"
#include "gskgpuimageprivate.h"
#include "gskgpunodeprocessorprivate.h"
#include "gskgpuopprivate.h"
#include "gskgpurendererprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"

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

  GskGpuOps ops;
  GskGpuOp *first_op;
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

  for (i = 0; i < gsk_gpu_ops_get_size (&priv->ops); i += op->op_class->size)
    {
      op = (GskGpuOp *) gsk_gpu_ops_index (&priv->ops, i);

      gsk_gpu_op_finish (op);
    }
  gsk_gpu_ops_set_size (&priv->ops, 0);
}

static void
gsk_gpu_frame_cleanup (GskGpuFrame *self)
{
  GSK_GPU_FRAME_GET_CLASS (self)->cleanup (self);
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

  g_object_unref (priv->device);

  G_OBJECT_CLASS (gsk_gpu_frame_parent_class)->finalize (object);
}

static void
gsk_gpu_frame_class_init (GskGpuFrameClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  klass->setup = gsk_gpu_frame_default_setup;
  klass->cleanup = gsk_gpu_frame_default_cleanup;

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
gsk_gpu_frame_setup (GskGpuFrame    *self,
                     GskGpuRenderer *renderer,
                     GskGpuDevice   *device)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  /* no reference, the renderer owns us */
  priv->renderer = renderer;
  priv->device = g_object_ref (device);

  GSK_GPU_FRAME_GET_CLASS (self)->setup (self);
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
          gsk_gpu_op_print (op, string, indent);
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
  guint i;

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
  while (op)
    {
      switch (op->op_class->stage)
      {
        case GSK_GPU_STAGE_UPLOAD:
          if (sort_data->upload.first == NULL)
            sort_data->upload.first = op;
          else
            sort_data->upload.last->next = op;
          sort_data->upload.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_COMMAND:
        case GSK_GPU_STAGE_SHADER:
          if (sort_data->command.first == NULL)
            sort_data->command.first = op;
          else
            sort_data->command.last->next = op;
          sort_data->command.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_PASS:
          if (sort_data->command.first == NULL)
            sort_data->command.first = op;
          else
            sort_data->command.last->next = op;
          sort_data->command.last = op;
          op = op->next;
          break;

        case GSK_GPU_STAGE_BEGIN_PASS:
          {
            SortData pass_data = { { NULL, NULL }, { op, op } };

            op = gsk_gpu_frame_sort_render_pass (self, op->next, &pass_data);

            if (pass_data.upload.first)
              {
                if (sort_data->upload.last == NULL)
                  sort_data->upload.last = pass_data.upload.last;
                else
                  pass_data.upload.last->next = sort_data->upload.first;
                sort_data->upload.first = pass_data.upload.first;
              }
            if (sort_data->command.last == NULL)
              sort_data->command.last = pass_data.command.last;
            else
              pass_data.command.last->next = sort_data->command.first;
            sort_data->command.first = pass_data.command.first;
          }
          break;

        case GSK_GPU_STAGE_END_PASS:
          sort_data->command.last->next = op;
          sort_data->command.last = op;
          return op->next;

        default:
          g_assert_not_reached ();
          break;
      }
    }

  return op;
}

static void
gsk_gpu_frame_sort_ops (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);
  SortData sort_data = { { NULL, }, };
  
  gsk_gpu_frame_sort_render_pass (self, priv->first_op, &sort_data);

  if (sort_data.upload.first)
    {
      sort_data.upload.last->next = sort_data.command.first;
      priv->first_op = sort_data.upload.first;
    }
  else
    priv->first_op = sort_data.command.first;
  if (sort_data.command.last)
    sort_data.command.last->next = NULL;
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

  return gsk_gpu_ops_index (&priv->ops, pos);
}

gboolean
gsk_gpu_frame_is_busy (GskGpuFrame *self)
{
  return GSK_GPU_FRAME_GET_CLASS (self)->is_busy (self);
}

static void
copy_texture (gpointer    user_data,
              GdkTexture *texture)
{
  GdkTexture **target = (GdkTexture **) user_data;

  *target = g_object_ref (texture);
}

static void
gsk_gpu_frame_record (GskGpuFrame            *self,
                      GskGpuImage            *target,
                      const cairo_region_t   *clip,
                      GskRenderNode          *node,
                      const graphene_rect_t  *viewport,
                      GdkTexture            **texture)
{
  cairo_rectangle_int_t extents;

  if (clip)
    {
      cairo_region_get_extents (clip, &extents);
    }
  else
    {
      extents = (cairo_rectangle_int_t) {
                    0, 0,
                    gsk_gpu_image_get_width (target),
                    gsk_gpu_image_get_height (target)
                };
    }

#if 0
  gsk_gpu_render_pass_begin_op (self,
                                target,
                                &extents,
                                VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#endif

  gsk_gpu_node_processor_process (self,
                                  target,
                                  &extents,
                                  node,
                                  viewport);

#if 0
  gsk_gpu_render_pass_end_op (self,
                              target,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
#endif

  if (texture)
    gsk_gpu_download_op (self, target, copy_texture, texture);
}

static void
gsk_gpu_frame_submit (GskGpuFrame *self)
{
  GskGpuFramePrivate *priv = gsk_gpu_frame_get_instance_private (self);

  gsk_gpu_frame_seal_ops (self);
  gsk_gpu_frame_verbose_print (self, "start of frame");
  gsk_gpu_frame_sort_ops (self);
  gsk_gpu_frame_verbose_print (self, "after sort");

  GSK_GPU_FRAME_GET_CLASS (self)->submit (self, priv->first_op);
}

void
gsk_gpu_frame_render (GskGpuFrame            *self,
                      GskGpuImage            *target,
                      const cairo_region_t   *region,
                      GskRenderNode          *node,
                      const graphene_rect_t  *viewport,
                      GdkTexture            **texture)
{
  gsk_gpu_frame_cleanup (self);

  gsk_gpu_frame_record (self, target, region, node, viewport, texture);

  gsk_gpu_frame_submit (self);
}


