#include "gskglrenderopsprivate.h"

void
ops_finish (RenderOpBuilder *builder)
{
  if (builder->mv_stack)
    g_array_free (builder->mv_stack, TRUE);
}


static inline void
rgba_to_float (const GdkRGBA *c,
               float         *f)
{
  f[0] = c->red;
  f[1] = c->green;
  f[2] = c->blue;
  f[3] = c->alpha;
}

float
ops_get_scale (const RenderOpBuilder *builder)
{
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  return head->metadata.scale;
}

static inline gboolean
matrix_is_only_translation (const graphene_matrix_t *mat)
{
  graphene_vec4_t row1;
  graphene_vec4_t row2;
  graphene_vec4_t row3;
  graphene_vec4_t row;

  graphene_vec4_init (&row1, 1, 0, 0, 0);
  graphene_vec4_init (&row2, 0, 1, 0, 0);
  graphene_vec4_init (&row3, 0, 0, 1, 0);

  graphene_matrix_get_row (mat, 0, &row);
  if (!graphene_vec4_equal (&row1, &row))
    return FALSE;

  graphene_matrix_get_row (mat, 1, &row);
  if (!graphene_vec4_equal (&row2, &row))
    return FALSE;

  graphene_matrix_get_row (mat, 2, &row);
  if (!graphene_vec4_equal (&row3, &row))
    return FALSE;

  graphene_matrix_get_row (mat, 3, &row);
  if (graphene_vec4_get_w (&row) != 1)
    return FALSE;

  return TRUE;
}

static void
extract_matrix_metadata (const graphene_matrix_t *m,
                         OpsMatrixMetadata       *md)
{
  graphene_vec3_t col1;
  graphene_vec3_t col2;

  /* Is this matrix JUST a translation? */
  md->is_only_translation = matrix_is_only_translation (m);

  /* TODO: We should probably split this up into two values... */

  /* Scale */
  graphene_vec3_init (&col1,
                      graphene_matrix_get_value (m, 0, 0),
                      graphene_matrix_get_value (m, 1, 0),
                      graphene_matrix_get_value (m, 2, 0));

  graphene_vec3_init (&col2,
                      graphene_matrix_get_value (m, 0, 1),
                      graphene_matrix_get_value (m, 1, 1),
                      graphene_matrix_get_value (m, 2, 1));

  md->scale = MAX (graphene_vec3_length (&col1),
                   graphene_vec3_length (&col2));

}


void
ops_transform_bounds_modelview (const RenderOpBuilder *builder,
                                const graphene_rect_t *src,
                                graphene_rect_t       *dst)
{
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  if (head->metadata.is_only_translation)
    {
      graphene_vec4_t row4;

      /* TODO: We could do the get_row here only once, when setting the new modelview matrix. */
      graphene_matrix_get_row (builder->current_modelview, 3, &row4);
      *dst = *src;
      graphene_rect_offset (dst, graphene_vec4_get_x (&row4), graphene_vec4_get_y (&row4));
    }
  else
    {
      graphene_matrix_transform_bounds (builder->current_modelview,
                                        src,
                                        dst);
    }

  graphene_rect_offset (dst, builder->dx, builder->dy);
}

void
ops_set_program (RenderOpBuilder *builder,
                 const Program   *program)
{
  /* The tricky part about this is that we want to initialize all uniforms of a program
   * to the current value from the builder, but only once. */
  static const GskRoundedRect empty_clip;
  static const graphene_matrix_t empty_matrix;
  static const graphene_rect_t empty_rect;
  RenderOp op;

  if (builder->current_program == program)
    return;

  op.op = OP_CHANGE_PROGRAM;
  op.program = program;
  g_array_append_val (builder->render_ops, op);
  builder->current_program = program;

  /* If the projection is not yet set for this program, we use the current one. */
  if (memcmp (&empty_matrix, &builder->program_state[program->index].projection, sizeof (graphene_matrix_t)) == 0 ||
      memcmp (&builder->current_projection, &builder->program_state[program->index].projection, sizeof (graphene_matrix_t)) != 0)
    {
      op.op = OP_CHANGE_PROJECTION;
      op.projection = builder->current_projection;
      g_array_append_val (builder->render_ops, op);
      builder->program_state[program->index].projection = builder->current_projection;
    }

  if (memcmp (&empty_matrix, &builder->program_state[program->index].modelview, sizeof (graphene_matrix_t)) == 0 ||
      memcmp (builder->current_modelview, &builder->program_state[program->index].modelview, sizeof (graphene_matrix_t)) != 0)
    {
      op.op = OP_CHANGE_MODELVIEW;
      op.modelview = *builder->current_modelview;
      g_array_append_val (builder->render_ops, op);
      builder->program_state[program->index].modelview = *builder->current_modelview;
    }

  if (memcmp (&empty_rect, &builder->program_state[program->index].viewport, sizeof (graphene_rect_t)) == 0 ||
      memcmp (&builder->current_viewport, &builder->program_state[program->index].viewport, sizeof (graphene_rect_t)) != 0)
    {
      op.op = OP_CHANGE_VIEWPORT;
      op.viewport = builder->current_viewport;
      g_array_append_val (builder->render_ops, op);
      builder->program_state[program->index].viewport = builder->current_viewport;
    }

  if (memcmp (&empty_clip, &builder->program_state[program->index].clip, sizeof (GskRoundedRect)) == 0 ||
      memcmp (&builder->current_clip, &builder->program_state[program->index].clip, sizeof (GskRoundedRect)) != 0)
    {
      op.op = OP_CHANGE_CLIP;
      op.clip = builder->current_clip;
      g_array_append_val (builder->render_ops, op);
      builder->program_state[program->index].clip = builder->current_clip;
    }

  if (builder->program_state[program->index].opacity != builder->current_opacity)
    {
      op.op = OP_CHANGE_OPACITY;
      op.opacity = builder->current_opacity;
      g_array_append_val (builder->render_ops, op);
      builder->program_state[program->index].opacity = builder->current_opacity;
    }
}

GskRoundedRect
ops_set_clip (RenderOpBuilder      *builder,
              const GskRoundedRect *clip)
{
  RenderOp *last_op;
  GskRoundedRect prev_clip;

  if (builder->render_ops->len > 0)
    {
      last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);

      if (last_op->op == OP_CHANGE_CLIP)
        {
          last_op->clip = *clip;
        }
      else
        {
          RenderOp op;

          op.op = OP_CHANGE_CLIP;
          op.clip = *clip;
          g_array_append_val (builder->render_ops, op);
        }
    }

  if (builder->current_program != NULL)
    builder->program_state[builder->current_program->index].clip = *clip;

  prev_clip = builder->current_clip;
  builder->current_clip = *clip;

  return prev_clip;
}

static void
ops_set_modelview (RenderOpBuilder         *builder,
                   const graphene_matrix_t *modelview)
{
  RenderOp op;

  if (builder->current_program &&
      memcmp (&builder->program_state[builder->current_program->index].modelview, modelview,
              sizeof (graphene_matrix_t)) == 0)
    return;

  if (builder->render_ops->len > 0)
    {
      RenderOp *last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);
      if (last_op->op == OP_CHANGE_MODELVIEW)
        {
          last_op->modelview = *modelview;
        }
      else
        {
          op.op = OP_CHANGE_MODELVIEW;
          op.modelview = *modelview;
          g_array_append_val (builder->render_ops, op);
        }
    }
  else
    {
      op.op = OP_CHANGE_MODELVIEW;
      op.modelview = *modelview;
      g_array_append_val (builder->render_ops, op);
    }

  if (builder->current_program != NULL)
    builder->program_state[builder->current_program->index].modelview = *modelview;
}

void
ops_push_modelview (RenderOpBuilder         *builder,
                    const graphene_matrix_t *mv)
{
  MatrixStackEntry *entry;

  if (G_UNLIKELY (builder->mv_stack == NULL))
      builder->mv_stack = g_array_new (FALSE, TRUE, sizeof (MatrixStackEntry));

  g_assert (builder->mv_stack != NULL);

  g_array_set_size (builder->mv_stack, builder->mv_stack->len + 1);
  entry = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  entry->matrix = *mv;
  extract_matrix_metadata (mv, &entry->metadata);

  builder->current_modelview = &entry->matrix;
  ops_set_modelview (builder, mv);
}

void
ops_pop_modelview (RenderOpBuilder *builder)
{
  const graphene_matrix_t *m;
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack);
  g_assert (builder->mv_stack->len >= 1);

  builder->mv_stack->len --;
  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);
  m = &head->matrix;

  if (builder->mv_stack->len >= 1)
    {
      builder->current_modelview = m;
      ops_set_modelview (builder, m);
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
  RenderOp op;
  graphene_matrix_t prev_mv;

  if (builder->render_ops->len > 0)
    {
      RenderOp *last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);
      if (last_op->op == OP_CHANGE_PROJECTION)
        {
          last_op->projection = *projection;
        }
      else
        {
          op.op = OP_CHANGE_PROJECTION;
          op.projection = *projection;
          g_array_append_val (builder->render_ops, op);
        }
    }
  else
    {
      op.op = OP_CHANGE_PROJECTION;
      op.projection = *projection;
      g_array_append_val (builder->render_ops, op);
    }

  if (builder->current_program != NULL)
    builder->program_state[builder->current_program->index].projection = *projection;

  prev_mv = builder->current_projection;
  builder->current_projection = *projection;

  return prev_mv;
}

graphene_rect_t
ops_set_viewport (RenderOpBuilder       *builder,
                  const graphene_rect_t *viewport)
{
  RenderOp op;
  graphene_rect_t prev_viewport;

  op.op = OP_CHANGE_VIEWPORT;
  op.viewport = *viewport;
  g_array_append_val (builder->render_ops, op);

  if (builder->current_program != NULL)
    builder->program_state[builder->current_program->index].viewport = *viewport;

  prev_viewport = builder->current_viewport;
  builder->current_viewport = *viewport;

  return prev_viewport;
}

void
ops_set_texture (RenderOpBuilder *builder,
                 int              texture_id)
{
  RenderOp op;

  if (builder->current_texture == texture_id)
    return;

  op.op = OP_CHANGE_SOURCE_TEXTURE;
  op.texture_id = texture_id;
  g_array_append_val (builder->render_ops, op);
  builder->current_texture = texture_id;
}

int
ops_set_render_target (RenderOpBuilder *builder,
                       int              render_target_id)
{
  RenderOp op;
  int prev_render_target;

  if (builder->current_render_target == render_target_id)
    return render_target_id;

  prev_render_target = builder->current_render_target;
  op.op = OP_CHANGE_RENDER_TARGET;
  op.render_target_id = render_target_id;
  g_array_append_val (builder->render_ops, op);
  builder->current_render_target = render_target_id;

  return prev_render_target;
}

float
ops_set_opacity (RenderOpBuilder *builder,
                 float            opacity)
{
  RenderOp op;
  float prev_opacity;
  RenderOp *last_op;

  if (builder->current_opacity == opacity)
    return opacity;

  if (builder->render_ops->len > 0)
    {
      last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);

      if (last_op->op == OP_CHANGE_OPACITY)
        {
          last_op->opacity = opacity;
        }
      else
        {
          op.op = OP_CHANGE_OPACITY;
          op.opacity = opacity;
          g_array_append_val (builder->render_ops, op);
        }
    }
  else
    {
      op.op = OP_CHANGE_OPACITY;
      op.opacity = opacity;
      g_array_append_val (builder->render_ops, op);
    }

  prev_opacity = builder->current_opacity;
  builder->current_opacity = opacity;

  if (builder->current_program != NULL)
    builder->program_state[builder->current_program->index].opacity = opacity;

  return prev_opacity;
}

void
ops_set_color (RenderOpBuilder *builder,
               const GdkRGBA   *color)
{
  RenderOp op;

  if (gdk_rgba_equal (color, &builder->program_state[builder->current_program->index].color))
    return;

  builder->program_state[builder->current_program->index].color = *color;

  op.op = OP_CHANGE_COLOR;
  op.color = *color;
  g_array_append_val (builder->render_ops, op);
}

void
ops_set_color_matrix (RenderOpBuilder         *builder,
                      const graphene_matrix_t *matrix,
                      const graphene_vec4_t   *offset)
{
  RenderOp op;

  if (memcmp (matrix,
              &builder->program_state[builder->current_program->index].color_matrix.matrix,
              sizeof (graphene_matrix_t)) == 0 &&
      memcmp (offset,
              &builder->program_state[builder->current_program->index].color_matrix.offset,
              sizeof (graphene_vec4_t)) == 0)
    return;

  builder->program_state[builder->current_program->index].color_matrix.matrix = *matrix;
  builder->program_state[builder->current_program->index].color_matrix.offset = *offset;

  op.op = OP_CHANGE_COLOR_MATRIX;
  op.color_matrix.matrix = *matrix;
  op.color_matrix.offset = *offset;
  g_array_append_val (builder->render_ops, op);
}

void
ops_set_border (RenderOpBuilder      *builder,
                const float          *widths,
                const GskRoundedRect *outline)
{
  RenderOp op;

  /* TODO: Assert that current_program == border program? */

  if (memcmp (&builder->program_state[builder->current_program->index].border.widths,
              widths, sizeof (float) * 4) == 0 &&
      memcmp (&builder->program_state[builder->current_program->index].border.outline,
              outline, sizeof (GskRoundedRect)) == 0)
    return;

  memcpy (&builder->program_state[builder->current_program->index].border.widths,
          widths, sizeof (float) * 4);

  builder->program_state[builder->current_program->index].border.outline = *outline;

  op.op = OP_CHANGE_BORDER;
  op.border.widths[0] = widths[0];
  op.border.widths[1] = widths[1];
  op.border.widths[2] = widths[2];
  op.border.widths[3] = widths[3];
  op.border.outline = *outline;
  g_array_append_val (builder->render_ops, op);
}

void
ops_set_border_color (RenderOpBuilder *builder,
                      const GdkRGBA   *color)
{
  RenderOp op;
  op.op = OP_CHANGE_BORDER_COLOR;
  rgba_to_float (color, op.border.color);

  if (memcmp (&op.border.color, &builder->program_state[builder->current_program->index].border.color,
              sizeof (float) * 4) == 0)
    return;

  rgba_to_float (color, builder->program_state[builder->current_program->index].border.color);

  g_array_append_val (builder->render_ops, op);
}

void
ops_draw (RenderOpBuilder     *builder,
          const GskQuadVertex  vertex_data[GL_N_VERTICES])
{
  RenderOp *last_op;

  last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);
  /* If the previous op was a DRAW as well, we didn't change anything between the two calls,
   * so these are just 2 subsequent draw calls. Same VAO, same program etc.
   * And the offsets into the vao are in order as well, so make it one draw call. */
  if (last_op->op == OP_DRAW)
    {
      /* We allow ourselves a little trick here. We still have to add a CHANGE_VAO op for
       * this draw call so we can add our vertex data there, but we want it to be placed before
       * the last draw call, so we reorder those. */
      RenderOp new_draw;
      new_draw.op = OP_DRAW;
      new_draw.draw.vao_offset = last_op->draw.vao_offset;
      new_draw.draw.vao_size = last_op->draw.vao_size + GL_N_VERTICES;

      last_op->op = OP_CHANGE_VAO;
      memcpy (&last_op->vertex_data, vertex_data, sizeof(GskQuadVertex) * GL_N_VERTICES);

      /* Now add the DRAW */
      g_array_append_val (builder->render_ops, new_draw);
    }
  else
    {
      const gsize n_ops = builder->render_ops->len;
      RenderOp *op;
      gsize offset = builder->buffer_size / sizeof (GskQuadVertex);

      /* We will add two render ops here. */
      g_array_set_size (builder->render_ops, n_ops + 2);

      op = &g_array_index (builder->render_ops, RenderOp, n_ops);
      op->op = OP_CHANGE_VAO;
      memcpy (&op->vertex_data, vertex_data, sizeof(GskQuadVertex) * GL_N_VERTICES);

      op = &g_array_index (builder->render_ops, RenderOp, n_ops + 1);
      op->op = OP_DRAW;
      op->draw.vao_offset = offset;
      op->draw.vao_size = GL_N_VERTICES;
    }

  /* We added new vertex data in both cases so increase the buffer size */
  builder->buffer_size += sizeof (GskQuadVertex) * GL_N_VERTICES;
}

void
ops_offset (RenderOpBuilder *builder,
            float            x,
            float            y)
{
  builder->dx += x;
  builder->dy += y;
}

void
ops_add (RenderOpBuilder *builder,
         const RenderOp  *op)
{
  g_array_append_val (builder->render_ops, *op);
}
