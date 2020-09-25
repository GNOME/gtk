#include "gskglrenderopsprivate.h"
#include "gsktransform.h"

typedef struct
{
  GskRoundedRect rect;
  bool is_rectilinear;
} ClipStackEntry;

static inline gboolean
rect_equal (const graphene_rect_t *a,
            const graphene_rect_t *b)
{
  return memcmp (a, b, sizeof (graphene_rect_t)) == 0;
}

static inline gboolean G_GNUC_PURE
rounded_rect_equal (const GskRoundedRect *r1,
                    const GskRoundedRect *r2)
{
  int i;

  if (!r1)
    return FALSE;

  if (r1->bounds.origin.x != r2->bounds.origin.x ||
      r1->bounds.origin.y != r2->bounds.origin.y ||
      r1->bounds.size.width != r2->bounds.size.width ||
      r1->bounds.size.height != r2->bounds.size.height)
    return FALSE;

  for (i = 0; i < 4; i ++)
    if (r1->corner[i].width != r2->corner[i].width ||
        r1->corner[i].height != r2->corner[i].height)
      return FALSE;

  return TRUE;
}

static inline gboolean G_GNUC_PURE
rounded_rect_corners_equal (const GskRoundedRect *r1,
                            const GskRoundedRect *r2)
{
  int i;

  if (!r1)
    return FALSE;

  for (i = 0; i < 4; i ++)
    if (r1->corner[i].width != r2->corner[i].width ||
        r1->corner[i].height != r2->corner[i].height)
      return FALSE;

  return TRUE;
}

static inline ProgramState *
get_current_program_state (RenderOpBuilder *builder)
{
  if (!builder->current_program)
    return NULL;

  return &builder->programs->state[builder->current_program->index];
}

void
ops_finish (RenderOpBuilder *builder)
{
  if (builder->mv_stack)
    g_array_free (builder->mv_stack, TRUE);
  builder->mv_stack = NULL;

  if (builder->clip_stack)
    g_array_free (builder->clip_stack, TRUE);
  builder->clip_stack = NULL;

  builder->dx = 0;
  builder->dy = 0;
  builder->scale_x = 1;
  builder->scale_y = 1;
  builder->current_modelview = NULL;
  builder->current_clip = NULL;
  builder->clip_is_rectilinear = TRUE;
  builder->current_render_target = 0;
  builder->current_texture = 0;
  builder->current_program = NULL;
  graphene_matrix_init_identity (&builder->current_projection);
  builder->current_viewport = GRAPHENE_RECT_INIT (0, 0, 0, 0);
}

/* Debugging only! */
void
ops_dump_framebuffer (RenderOpBuilder *builder,
                      const char      *filename,
                      int              width,
                      int              height)
{
  OpDumpFrameBuffer *op;

  op = ops_begin (builder, OP_DUMP_FRAMEBUFFER);
  op->filename = g_strdup (filename);
  op->width = width;
  op->height = height;
}

void
ops_push_debug_group (RenderOpBuilder *builder,
                      const char      *text)
{
  OpDebugGroup *op;

  op = ops_begin (builder, OP_PUSH_DEBUG_GROUP);
  strncpy (op->text, text, sizeof(op->text) - 1);
  op->text[sizeof(op->text) - 1] = 0; /* Ensure zero terminated */
}

void
ops_pop_debug_group (RenderOpBuilder *builder)
{
  ops_begin (builder, OP_POP_DEBUG_GROUP);
}

float
ops_get_scale (const RenderOpBuilder *builder)
{
  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  /* TODO: Use two separate values */
  return MAX (builder->scale_x, builder->scale_y);
}

static void
extract_matrix_metadata (GskTransform      *transform,
                         OpsMatrixMetadata *md)
{
  float dummy;

  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      md->scale_x = 1;
      md->scale_y = 1;
    break;

    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      gsk_transform_to_affine (transform,
                               &md->scale_x, &md->scale_y,
                               &dummy, &dummy);
    break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
      {
        graphene_vec3_t col1;
        graphene_vec3_t col2;
        graphene_matrix_t m;

        gsk_transform_to_matrix (transform, &m);

        /* TODO: 90% sure this is incorrect. But we should never hit this code
         * path anyway. */
        graphene_vec3_init (&col1,
                            graphene_matrix_get_value (&m, 0, 0),
                            graphene_matrix_get_value (&m, 1, 0),
                            graphene_matrix_get_value (&m, 2, 0));

        graphene_vec3_init (&col2,
                            graphene_matrix_get_value (&m, 0, 1),
                            graphene_matrix_get_value (&m, 1, 1),
                            graphene_matrix_get_value (&m, 2, 1));

        md->scale_x = graphene_vec3_length (&col1);
        md->scale_y = graphene_vec3_length (&col2);
      }
    break;
    default:
      {}
    }
}

void
ops_transform_bounds_modelview (const RenderOpBuilder *builder,
                                const graphene_rect_t *src,
                                graphene_rect_t       *dst)
{
  graphene_rect_t r = *src;

  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  r.origin.x += builder->dx;
  r.origin.y += builder->dy;

  gsk_transform_transform_bounds (builder->current_modelview, &r, dst);
}

void
ops_init (RenderOpBuilder *builder)
{
  memset (builder, 0, sizeof (*builder));

  builder->current_opacity = 1.0f;

  op_buffer_init (&builder->render_ops);
  builder->vertices = g_array_new (FALSE, TRUE, sizeof (GskQuadVertex));
}

void
ops_free (RenderOpBuilder *builder)
{
  g_array_unref (builder->vertices);
  op_buffer_destroy (&builder->render_ops);
}

void
ops_set_program (RenderOpBuilder *builder,
                 const Program   *program)
{
  OpProgram *op;
  ProgramState *program_state;

  if (builder->current_program == program)
    return;

  op = ops_begin (builder, OP_CHANGE_PROGRAM);
  op->program = program;

  builder->current_program = program;

  program_state = &builder->programs->state[program->index];

  if (memcmp (&builder->current_projection, &program_state->projection, sizeof (graphene_matrix_t)) != 0)
    {
      OpMatrix *opm;

      opm = ops_begin (builder, OP_CHANGE_PROJECTION);
      opm->matrix = builder->current_projection;
      program_state->projection = builder->current_projection;
    }

  if (program_state->modelview == NULL ||
      !gsk_transform_equal (builder->current_modelview, program_state->modelview))
    {
      OpMatrix *opm;

      opm = ops_begin (builder, OP_CHANGE_MODELVIEW);
      gsk_transform_to_matrix (builder->current_modelview, &opm->matrix);
      gsk_transform_unref (program_state->modelview);
      program_state->modelview = gsk_transform_ref (builder->current_modelview);
    }

  if (!rect_equal (&builder->current_viewport, &program_state->viewport))
    {
      OpViewport *opv;

      opv = ops_begin (builder, OP_CHANGE_VIEWPORT);
      opv->viewport = builder->current_viewport;
      program_state->viewport = builder->current_viewport;
    }

  if (!rounded_rect_equal (builder->current_clip, &program_state->clip))
    {
      OpClip *opc;

      opc = ops_begin (builder, OP_CHANGE_CLIP);
      opc->clip = *builder->current_clip;
      opc->send_corners = !rounded_rect_corners_equal (builder->current_clip, &program_state->clip);
      program_state->clip = *builder->current_clip;
    }

  if (program_state->opacity != builder->current_opacity)
    {
      OpOpacity *opo;

      opo = ops_begin (builder, OP_CHANGE_OPACITY);
      opo->opacity = builder->current_opacity;
      program_state->opacity = builder->current_opacity;
    }
}

static void
ops_set_clip (RenderOpBuilder      *builder,
              const GskRoundedRect *clip)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpClip *op;

  if (current_program_state &&
      rounded_rect_equal (&current_program_state->clip, clip))
    return;

  if (!(op = op_buffer_peek_tail_checked (&builder->render_ops, OP_CHANGE_CLIP)))
    {
      op = op_buffer_add (&builder->render_ops, OP_CHANGE_CLIP);
      op->send_corners = !current_program_state ||
                         !rounded_rect_corners_equal (&current_program_state->clip, clip);
    }
  else
    {
      /* If the op before sent the corners, this one needs, too */
      op->send_corners |= !current_program_state ||
                          !rounded_rect_corners_equal (&current_program_state->clip, clip);
    }

  op->clip = *clip;

  if (current_program_state)
    current_program_state->clip = *clip;
}

void
ops_push_clip (RenderOpBuilder      *self,
               const GskRoundedRect *clip)
{
  ClipStackEntry entry;

  if (G_UNLIKELY (self->clip_stack == NULL))
    self->clip_stack = g_array_new (FALSE, TRUE, sizeof (ClipStackEntry));

  g_assert (self->clip_stack != NULL);

  entry.rect = *clip;
  entry.is_rectilinear = gsk_rounded_rect_is_rectilinear (clip);
  g_array_append_val (self->clip_stack, entry);
  self->current_clip = &g_array_index (self->clip_stack, ClipStackEntry, self->clip_stack->len - 1).rect;
  self->clip_is_rectilinear = entry.is_rectilinear;
  ops_set_clip (self, clip);
}

void
ops_pop_clip (RenderOpBuilder *self)
{
  const ClipStackEntry *head;

  g_assert (self->clip_stack);
  g_assert (self->clip_stack->len >= 1);

  self->clip_stack->len --;
  head = &g_array_index (self->clip_stack, ClipStackEntry, self->clip_stack->len - 1);

  if (self->clip_stack->len >= 1)
    {
      self->current_clip = &head->rect;
      self->clip_is_rectilinear = head->is_rectilinear;
      ops_set_clip (self, &head->rect);
    }
  else
    {
      self->current_clip = NULL;
      self->clip_is_rectilinear = TRUE;
    }
}

gboolean
ops_has_clip (RenderOpBuilder *self)
{
  return self->clip_stack != NULL &&
         self->clip_stack->len > 1;
}

static void
ops_set_modelview_internal (RenderOpBuilder *builder,
                            GskTransform    *transform)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpMatrix *op;

#if 0
  XXX This is not possible if we want pop() to work.
  if (builder->current_program &&
      gsk_transform_equal (builder->current_program_state->modelview, transform))
    return;
#endif

  if (!(op = op_buffer_peek_tail_checked (&builder->render_ops, OP_CHANGE_MODELVIEW)))
    op = op_buffer_add (&builder->render_ops, OP_CHANGE_MODELVIEW);

  gsk_transform_to_matrix (transform, &op->matrix);

  if (builder->current_program != NULL)
    {
      gsk_transform_unref (current_program_state->modelview);
      current_program_state->modelview = gsk_transform_ref (transform);
    }
}

/**
 * ops_set_modelview:
 * @builder
 * @transform: (transfer full): The new modelview transform
 *
 * This sets the modelview to the given one without looking at the
 * one that's currently set */
void
ops_set_modelview (RenderOpBuilder *builder,
                   GskTransform    *transform)
{
  MatrixStackEntry *entry;

  if (G_UNLIKELY (builder->mv_stack == NULL))
    builder->mv_stack = g_array_new (FALSE, TRUE, sizeof (MatrixStackEntry));

  g_assert (builder->mv_stack != NULL);

  g_array_set_size (builder->mv_stack, builder->mv_stack->len + 1);
  entry = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  entry->transform = transform;

  entry->metadata.dx_before = builder->dx;
  entry->metadata.dy_before = builder->dy;
  extract_matrix_metadata (entry->transform, &entry->metadata);

  builder->dx = 0;
  builder->dy = 0;
  builder->current_modelview = entry->transform;
  builder->scale_x = entry->metadata.scale_x;
  builder->scale_y = entry->metadata.scale_y;
  ops_set_modelview_internal (builder, entry->transform);
}

/* This sets the given modelview to the one we get when multiplying
 * the given modelview with the current one. */
void
ops_push_modelview (RenderOpBuilder *builder,
                    GskTransform    *transform)
{
  MatrixStackEntry *entry;

  if (G_UNLIKELY (builder->mv_stack == NULL))
    builder->mv_stack = g_array_new (FALSE, TRUE, sizeof (MatrixStackEntry));

  g_assert (builder->mv_stack != NULL);

  g_array_set_size (builder->mv_stack, builder->mv_stack->len + 1);
  entry = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  if (G_LIKELY (builder->mv_stack->len >= 2))
    {
      const MatrixStackEntry *cur;
      GskTransform *t = NULL;

      cur = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 2);
      /* Multiply given matrix with current modelview */

      t = gsk_transform_translate (gsk_transform_ref (cur->transform),
                                   &(graphene_point_t) { builder->dx, builder->dy});
      t = gsk_transform_transform (t, transform);
      entry->transform = t;
    }
  else
    {
      entry->transform = gsk_transform_ref (transform);
    }

  entry->metadata.dx_before = builder->dx;
  entry->metadata.dy_before = builder->dy;
  extract_matrix_metadata (entry->transform, &entry->metadata);

  builder->dx = 0;
  builder->dy = 0;
  builder->scale_x = entry->metadata.scale_x;
  builder->scale_y = entry->metadata.scale_y;
  builder->current_modelview = entry->transform;
  ops_set_modelview_internal (builder, entry->transform);
}

void
ops_pop_modelview (RenderOpBuilder *builder)
{
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack);
  g_assert (builder->mv_stack->len >= 1);

  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);
  builder->dx = head->metadata.dx_before;
  builder->dy = head->metadata.dy_before;
  gsk_transform_unref (head->transform);

  builder->mv_stack->len --;
  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  if (builder->mv_stack->len >= 1)
    {
      builder->scale_x = head->metadata.scale_x;
      builder->scale_y = head->metadata.scale_y;
      builder->current_modelview = head->transform;
      ops_set_modelview_internal (builder, head->transform);
    }
  else
    {
      builder->current_modelview = NULL;
    }
}

graphene_matrix_t
ops_set_projection (RenderOpBuilder         *builder,
                    const graphene_matrix_t *projection)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  graphene_matrix_t prev_mv;
  OpMatrix *op;

  if (!(op = op_buffer_peek_tail_checked (&builder->render_ops, OP_CHANGE_PROJECTION)))
    op = op_buffer_add (&builder->render_ops, OP_CHANGE_PROJECTION);

  op->matrix = *projection;

  if (builder->current_program != NULL)
    current_program_state->projection = *projection;

  prev_mv = builder->current_projection;
  builder->current_projection = *projection;

  return prev_mv;
}

graphene_rect_t
ops_set_viewport (RenderOpBuilder       *builder,
                  const graphene_rect_t *viewport)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpViewport *op;
  graphene_rect_t prev_viewport;

  if (current_program_state != NULL &&
      rect_equal (&current_program_state->viewport, viewport))
    return current_program_state->viewport;

  op = ops_begin (builder, OP_CHANGE_VIEWPORT);
  op->viewport = *viewport;

  if (current_program_state != NULL)
    current_program_state->viewport = *viewport;

  prev_viewport = builder->current_viewport;
  builder->current_viewport = *viewport;

  return prev_viewport;
}

void
ops_set_texture (RenderOpBuilder *builder,
                 int              texture_id)
{
  OpTexture *op;

  if (builder->current_texture == texture_id)
    return;

  op = ops_begin (builder, OP_CHANGE_SOURCE_TEXTURE);
  op->texture_id = texture_id;
  builder->current_texture = texture_id;
}

int
ops_set_render_target (RenderOpBuilder *builder,
                       int              render_target_id)
{
  OpRenderTarget *op;
  int prev_render_target;

  if (builder->current_render_target == render_target_id)
    return render_target_id;

  prev_render_target = builder->current_render_target;

  if (!(op = op_buffer_peek_tail_checked (&builder->render_ops, OP_CHANGE_RENDER_TARGET)))
    op = op_buffer_add (&builder->render_ops, OP_CHANGE_RENDER_TARGET);

  op->render_target_id = render_target_id;

  builder->current_render_target = render_target_id;

  return prev_render_target;
}

float
ops_set_opacity (RenderOpBuilder *builder,
                 float            opacity)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpOpacity *op;
  float prev_opacity;

  if (builder->current_opacity == opacity)
    return opacity;

  if (!(op = op_buffer_peek_tail_checked (&builder->render_ops, OP_CHANGE_OPACITY)))
    op = op_buffer_add (&builder->render_ops, OP_CHANGE_OPACITY);

  op->opacity = opacity;

  prev_opacity = builder->current_opacity;
  builder->current_opacity = opacity;

  if (builder->current_program != NULL)
    current_program_state->opacity = opacity;

  return prev_opacity;
}

void
ops_set_color (RenderOpBuilder *builder,
               const GdkRGBA   *color)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpColor *op;

  if (gdk_rgba_equal (color, &current_program_state->color))
    return;

  current_program_state->color = *color;

  op = ops_begin (builder, OP_CHANGE_COLOR);
  op->rgba = color;
}

void
ops_set_color_matrix (RenderOpBuilder         *builder,
                      const graphene_matrix_t *matrix,
                      const graphene_vec4_t   *offset)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpColorMatrix *op;
  bool offset_equal;

  offset_equal = memcmp (offset,
                         &current_program_state->color_matrix.offset,
                         sizeof (graphene_vec4_t)) == 0;

  if (memcmp (matrix,
              &current_program_state->color_matrix.matrix,
              sizeof (graphene_matrix_t)) == 0 &&
      offset_equal)
    return;

  current_program_state->color_matrix.matrix = *matrix;

  op = ops_begin (builder, OP_CHANGE_COLOR_MATRIX);
  op->matrix = matrix;

  if (!offset_equal)
    {
      op->offset.value = offset;
      op->offset.send = TRUE;

      current_program_state->color_matrix.offset = *offset;
    }
  else
    op->offset.send = FALSE;
}

void
ops_set_border (RenderOpBuilder      *builder,
                const GskRoundedRect *outline)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpBorder *op;

  if (memcmp (&current_program_state->border.outline,
              outline, sizeof (GskRoundedRect)) == 0)
    return;

  current_program_state->border.outline = *outline;

  op = ops_begin (builder, OP_CHANGE_BORDER);
  op->outline = *outline;
}

void
ops_set_border_width (RenderOpBuilder *builder,
                      const float     *widths)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpBorder *op;

  g_assert (current_program_state);

  if (memcmp (current_program_state->border.widths,
              widths, sizeof (float) * 4) == 0)
    return;

  memcpy (&current_program_state->border.widths,
          widths, sizeof (float) * 4);

  op = ops_begin (builder, OP_CHANGE_BORDER_WIDTH);
  op->widths[0] = widths[0];
  op->widths[1] = widths[1];
  op->widths[2] = widths[2];
  op->widths[3] = widths[3];
}

void
ops_set_border_color (RenderOpBuilder *builder,
                      const GdkRGBA   *color)
{
  ProgramState *current_program_state = get_current_program_state (builder);
  OpBorder *op;

  if (gdk_rgba_equal (color, &current_program_state->border.color))
    return;

  op = op_buffer_add (&builder->render_ops, OP_CHANGE_BORDER_COLOR);
  op->color = color;

  current_program_state->border.color = *color;
}

GskQuadVertex *
ops_draw (RenderOpBuilder     *builder,
          const GskQuadVertex  vertex_data[GL_N_VERTICES])
{
  OpDraw *op;

  if ((op = op_buffer_peek_tail_checked (&builder->render_ops, OP_DRAW)))
    {
      op->vao_size += GL_N_VERTICES;
    }
  else
    {
      op = op_buffer_add (&builder->render_ops, OP_DRAW);
      op->vao_offset = builder->vertices->len;
      op->vao_size = GL_N_VERTICES;
    }

  if (vertex_data)
    {
      g_array_append_vals (builder->vertices, vertex_data, GL_N_VERTICES);
      return NULL; /* Better not use this on the caller side */
    }

  g_array_set_size (builder->vertices, builder->vertices->len + GL_N_VERTICES);
  return &g_array_index (builder->vertices, GskQuadVertex, builder->vertices->len - GL_N_VERTICES);
}

/* The offset is only valid for the current modelview.
 * Setting a new modelview will add the offset to that matrix
 * and reset the internal offset to 0. */
void
ops_offset (RenderOpBuilder *builder,
            float            x,
            float            y)
{
  builder->dx += x;
  builder->dy += y;
}

gpointer
ops_begin (RenderOpBuilder *builder,
           OpKind           kind)
{
  return op_buffer_add (&builder->render_ops, kind);
}

void
ops_reset (RenderOpBuilder *builder)
{
  op_buffer_clear (&builder->render_ops);
  g_array_set_size (builder->vertices, 0);
}

OpBuffer *
ops_get_buffer (RenderOpBuilder *builder)
{
  return &builder->render_ops;
}

void
ops_set_inset_shadow (RenderOpBuilder      *self,
                      const GskRoundedRect  outline,
                      float                 spread,
                      const GdkRGBA        *color,
                      float                 dx,
                      float                 dy)
{
  ProgramState *current_program_state = get_current_program_state (self);
  OpShadow *op;

  g_assert (current_program_state);

  op = ops_begin (self, OP_CHANGE_INSET_SHADOW);

  if (!rounded_rect_equal (&outline, &current_program_state->inset_shadow.outline))
    {
      op->outline.value = outline;
      op->outline.send = TRUE;
      op->outline.send_corners = !rounded_rect_corners_equal (&current_program_state->inset_shadow.outline,
                                                              &outline);
      current_program_state->inset_shadow.outline = outline;
    }
  else
    op->outline.send = FALSE;

  if (spread != current_program_state->inset_shadow.spread)
    {
      op->spread.value = spread;
      op->spread.send = TRUE;

      current_program_state->inset_shadow.spread = spread;
    }
  else
    op->spread.send = FALSE;

  if (!gdk_rgba_equal (color, &current_program_state->inset_shadow.color))
    {
      op->color.value = color;
      op->color.send = TRUE;

      current_program_state->inset_shadow.color = *color;
    }
  else
    op->color.send = FALSE;

  if (dx != current_program_state->inset_shadow.dx ||
      dy != current_program_state->inset_shadow.dy)
    {
      op->offset.value[0] = dx;
      op->offset.value[1] = dy;
      op->offset.send = TRUE;

      current_program_state->inset_shadow.dx = dx;
      current_program_state->inset_shadow.dy = dy;
    }
  else
    op->offset.send = FALSE;

  if (!op->outline.send &&
      !op->spread.send &&
      !op->offset.send &&
      !op->color.send)
    {
      op_buffer_pop_tail (&self->render_ops);
    }
}
void
ops_set_unblurred_outset_shadow (RenderOpBuilder      *self,
                                 const GskRoundedRect  outline,
                                 float                 spread,
                                 const GdkRGBA        *color,
                                 float                 dx,
                                 float                 dy)
{
  ProgramState *current_program_state = get_current_program_state (self);
  OpShadow *op;

  g_assert (current_program_state);

  op = ops_begin (self, OP_CHANGE_UNBLURRED_OUTSET_SHADOW);

  if (!rounded_rect_equal (&outline, &current_program_state->unblurred_outset_shadow.outline))
    {
      op->outline.value = outline;
      op->outline.send = TRUE;
      op->outline.send_corners = !rounded_rect_corners_equal (&current_program_state->unblurred_outset_shadow.outline,
                                                              &outline);
      current_program_state->unblurred_outset_shadow.outline = outline;
    }
  else
    op->outline.send = FALSE;

  if (spread != current_program_state->unblurred_outset_shadow.spread)
    {
      op->spread.value = spread;
      op->spread.send = TRUE;

      current_program_state->unblurred_outset_shadow.spread = spread;
    }
  else
    op->spread.send = FALSE;

  if (!gdk_rgba_equal (color, &current_program_state->unblurred_outset_shadow.color))
    {
      op->color.value = color;
      op->color.send = TRUE;

      current_program_state->unblurred_outset_shadow.color = *color;
    }
  else
    op->color.send = FALSE;

  if (dx != current_program_state->unblurred_outset_shadow.dx ||
      dy != current_program_state->unblurred_outset_shadow.dy)
    {
      op->offset.value[0] = dx;
      op->offset.value[1] = dy;
      op->offset.send = TRUE;

      current_program_state->unblurred_outset_shadow.dx = dx;
      current_program_state->unblurred_outset_shadow.dy = dy;
    }
  else
    op->offset.send = FALSE;
}

void
ops_set_linear_gradient (RenderOpBuilder     *self,
                         guint                n_color_stops,
                         const GskColorStop  *color_stops,
                         float                start_x,
                         float                start_y,
                         float                end_x,
                         float                end_y)
{
  ProgramState *current_program_state = get_current_program_state (self);
  OpLinearGradient *op;
  const guint real_n_color_stops = MIN (GL_MAX_GRADIENT_STOPS, n_color_stops);

  g_assert (current_program_state);

  op = ops_begin (self, OP_CHANGE_LINEAR_GRADIENT);

  /* We always save the n_color_stops value in the op so the renderer can use it in
   * cases where we send the color stops, but not n_color_stops */
  op->n_color_stops.value = real_n_color_stops;
  if (current_program_state->linear_gradient.n_color_stops != real_n_color_stops)
    {
      op->n_color_stops.send = TRUE;
      current_program_state->linear_gradient.n_color_stops = real_n_color_stops;
    }
  else
    op->n_color_stops.send = FALSE;

  op->color_stops.send = FALSE;
  if (!op->n_color_stops.send)
    {
      g_assert (current_program_state->linear_gradient.n_color_stops == real_n_color_stops);

      for (guint i = 0; i < real_n_color_stops; i ++)
        {
          const GskColorStop *s1 = &color_stops[i];
          const GskColorStop *s2 = &current_program_state->linear_gradient.color_stops[i];

          if (s1->offset != s2->offset ||
              !gdk_rgba_equal (&s1->color, &s2->color))
            {
              op->color_stops.send = TRUE;
              break;
            }
        }
    }
  else
    op->color_stops.send = TRUE;

  if (op->color_stops.send)
    {
      op->color_stops.value = color_stops;
      memcpy (&current_program_state->linear_gradient.color_stops,
              color_stops,
              sizeof (GskColorStop) * real_n_color_stops);
    }

  op->start_point[0] = start_x;
  op->start_point[1] = start_y;
  op->end_point[0] = end_x;
  op->end_point[1] = end_y;
}

void
ops_set_radial_gradient (RenderOpBuilder    *self,
                         guint               n_color_stops,
                         const GskColorStop *color_stops,
                         float               center_x,
                         float               center_y,
                         float               start,
                         float               end,
                         float               hradius,
                         float               vradius)
{
  const guint real_n_color_stops = MIN (GL_MAX_GRADIENT_STOPS, n_color_stops);
  OpRadialGradient *op;

  /* TODO: State tracking? */

  op = ops_begin (self, OP_CHANGE_RADIAL_GRADIENT);
  op->n_color_stops.value = real_n_color_stops;
  op->n_color_stops.send = true;
  op->color_stops.value = color_stops;
  op->color_stops.send = true;
  op->center[0] = center_x;
  op->center[1] = center_y;
  op->radius[0] = hradius;
  op->radius[1] = vradius;
  op->start = start;
  op->end = end;
}
