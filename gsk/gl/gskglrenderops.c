#include "gskglrenderopsprivate.h"
#include "gsktransform.h"

static inline gboolean
rect_equal (const graphene_rect_t *a,
            const graphene_rect_t *b)
{
  return memcmp (a, b, sizeof (graphene_rect_t)) == 0;
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

  builder->buffer_size = 0;
  builder->dx = 0;
  builder->dy = 0;
  builder->current_modelview = NULL;
  builder->current_clip = NULL;
  builder->current_render_target = 0;
  builder->current_texture = 0;
  builder->current_program = NULL;
  builder->current_program_state = NULL;
  graphene_matrix_init_identity (&builder->current_projection);
  builder->current_viewport = GRAPHENE_RECT_INIT (0, 0, 0, 0);
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

/* Debugging only! */
void
ops_dump_framebuffer (RenderOpBuilder *builder,
                      const char      *filename,
                      int              width,
                      int              height)
{
  RenderOp op;

  op.op = OP_DUMP_FRAMEBUFFER;
  op.dump.filename = g_strdup (filename);
  op.dump.width = width;
  op.dump.height = height;

  g_array_append_val (builder->render_ops, op);
}

void
ops_push_debug_group (RenderOpBuilder *builder,
                      const char      *text)
{
  RenderOp op;

  op.op = OP_PUSH_DEBUG_GROUP;
  strncpy (op.debug_group.text, text, sizeof(op.debug_group.text) - 1);
  op.debug_group.text[sizeof(op.debug_group.text) - 1] = 0; /* Ensure zero terminated */

  g_array_append_val (builder->render_ops, op);
}

void
ops_pop_debug_group (RenderOpBuilder *builder)
{
  RenderOp op;

  op.op = OP_POP_DEBUG_GROUP;
  g_array_append_val (builder->render_ops, op);
}

float
ops_get_scale (const RenderOpBuilder *builder)
{
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  /* TODO: Use two separate values */
  return MAX (head->metadata.scale_x,
              head->metadata.scale_y);
}

static void
extract_matrix_metadata (GskTransform      *transform,
                         OpsMatrixMetadata *md)
{
  switch (gsk_transform_get_category (transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      md->scale_x = 1;
      md->scale_y = 1;
    break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      gsk_transform_to_translate (transform, &md->translate_x, &md->translate_y);
      md->scale_x = 1;
      md->scale_y = 1;
    break;

    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
      {
        graphene_vec3_t col1;
        graphene_vec3_t col2;
        graphene_matrix_t m;

        gsk_transform_to_matrix (transform, &m);

        /* TODO: Is this event possible (or correct) now that we use a GskTransform here? */

        md->translate_x = graphene_matrix_get_value (&m, 3, 0);
        md->translate_y = graphene_matrix_get_value (&m, 3, 1);

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
  const MatrixStackEntry *head;

  g_assert (builder->mv_stack != NULL);
  g_assert (builder->mv_stack->len >= 1);

  head = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  switch (gsk_transform_get_category (head->transform))
    {
    case GSK_TRANSFORM_CATEGORY_IDENTITY:
      *dst = *src;
      break;

    case GSK_TRANSFORM_CATEGORY_2D_TRANSLATE:
      *dst = *src;
      dst->origin.x += head->metadata.translate_x;
      dst->origin.y += head->metadata.translate_y;
      break;

    /* TODO: Handle scale */
    case GSK_TRANSFORM_CATEGORY_UNKNOWN:
    case GSK_TRANSFORM_CATEGORY_ANY:
    case GSK_TRANSFORM_CATEGORY_3D:
    case GSK_TRANSFORM_CATEGORY_2D:
    case GSK_TRANSFORM_CATEGORY_2D_AFFINE:
    default:
      gsk_transform_transform_bounds (builder->current_modelview, src, dst);
    }

  dst->origin.x += builder->dx * head->metadata.scale_x;
  dst->origin.y += builder->dy * head->metadata.scale_y;
}

void
ops_init (RenderOpBuilder *builder)
{
  int i;

  memset (builder, 0, sizeof (*builder));

  builder->current_opacity = 1.0f;

  for (i = 0; i < GL_N_PROGRAMS; i ++)
    {
      builder->program_state[i].opacity = 1.0f;
    }
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
  ProgramState *program_state;

  if (builder->current_program == program)
    return;

  op.op = OP_CHANGE_PROGRAM;
  op.program = program;
  g_array_append_val (builder->render_ops, op);
  builder->current_program = program;

  program_state = &builder->program_state[program->index];

  /* If the projection is not yet set for this program, we use the current one. */
  if (memcmp (&empty_matrix, &program_state->projection, sizeof (graphene_matrix_t)) == 0 ||
      memcmp (&builder->current_projection, &program_state->projection, sizeof (graphene_matrix_t)) != 0)
    {
      op.op = OP_CHANGE_PROJECTION;
      op.projection = builder->current_projection;
      g_array_append_val (builder->render_ops, op);
      program_state->projection = builder->current_projection;
    }

  if (memcmp (&empty_matrix, &program_state->modelview, sizeof (graphene_matrix_t)) == 0 ||
      !gsk_transform_equal (builder->current_modelview, program_state->modelview))
    {
      op.op = OP_CHANGE_MODELVIEW;
      gsk_transform_to_matrix (builder->current_modelview, &op.modelview);
      g_array_append_val (builder->render_ops, op);
      program_state->modelview = gsk_transform_ref (builder->current_modelview);
    }

  if (rect_equal (&empty_rect, &program_state->viewport) ||
      !rect_equal (&builder->current_viewport, &program_state->viewport))
    {
      op.op = OP_CHANGE_VIEWPORT;
      op.viewport = builder->current_viewport;
      g_array_append_val (builder->render_ops, op);
      program_state->viewport = builder->current_viewport;
    }

  if (memcmp (&empty_clip, &program_state->clip, sizeof (GskRoundedRect)) == 0 ||
      memcmp (&builder->current_clip, &program_state->clip, sizeof (GskRoundedRect)) != 0)
    {
      op.op = OP_CHANGE_CLIP;
      op.clip = *builder->current_clip;
      g_array_append_val (builder->render_ops, op);
      program_state->clip = *builder->current_clip;
    }

  if (program_state->opacity != builder->current_opacity)
    {
      op.op = OP_CHANGE_OPACITY;
      op.opacity = builder->current_opacity;
      g_array_append_val (builder->render_ops, op);
      program_state->opacity = builder->current_opacity;
    }

  builder->current_program_state = &builder->program_state[program->index];
}

static void
ops_set_clip (RenderOpBuilder      *builder,
              const GskRoundedRect *clip)
{
  RenderOp *last_op;

  if (builder->current_program_state &&
      memcmp (&builder->current_program_state->clip, clip,sizeof (GskRoundedRect)) == 0)
    return;

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
    builder->current_program_state->clip = *clip;
}

void
ops_push_clip (RenderOpBuilder      *self,
               const GskRoundedRect *clip)
{
  if (G_UNLIKELY (self->clip_stack == NULL))
    self->clip_stack = g_array_new (FALSE, TRUE, sizeof (GskRoundedRect));

  g_assert (self->clip_stack != NULL);

  g_array_append_val (self->clip_stack, *clip);
  self->current_clip = &g_array_index (self->clip_stack, GskRoundedRect, self->clip_stack->len - 1);
  ops_set_clip (self, clip);
}

void
ops_pop_clip (RenderOpBuilder *self)
{
  const GskRoundedRect *head;

  g_assert (self->clip_stack);
  g_assert (self->clip_stack->len >= 1);

  self->clip_stack->len --;
  head = &g_array_index (self->clip_stack, GskRoundedRect, self->clip_stack->len - 1);

  if (self->clip_stack->len >= 1)
    {
      self->current_clip = head;
      ops_set_clip (self, head);
    }
  else
    {
      self->current_clip = NULL;
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
  RenderOp op;
  graphene_matrix_t matrix;

  gsk_transform_to_matrix (transform, &matrix);

  if (builder->current_program &&
      gsk_transform_equal (builder->current_program_state->modelview, transform))
    return;

  if (builder->render_ops->len > 0)
    {
      RenderOp *last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);
      if (last_op->op == OP_CHANGE_MODELVIEW)
        {
          last_op->modelview = matrix;
        }
      else
        {
          op.op = OP_CHANGE_MODELVIEW;
          op.modelview = matrix;
          g_array_append_val (builder->render_ops, op);
        }
    }
  else
    {
      op.op = OP_CHANGE_MODELVIEW;
      op.modelview = matrix;
      g_array_append_val (builder->render_ops, op);
    }

  if (builder->current_program != NULL)
    builder->current_program_state->modelview = gsk_transform_ref (transform);
}

/* This sets the modelview to the given one without looking at the
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
  extract_matrix_metadata (transform, &entry->metadata);

  builder->dx = 0;
  builder->dy = 0;
  builder->current_modelview = entry->transform;
  ops_set_modelview_internal (builder, entry->transform);
}

/* This sets the given modelview to the one we get when multiplying
 * the given modelview with the current one. */
void
ops_push_modelview (RenderOpBuilder *builder,
                    GskTransform    *transform)
{
  float scale = ops_get_scale (builder);
  MatrixStackEntry *entry;

  if (G_UNLIKELY (builder->mv_stack == NULL))
    builder->mv_stack = g_array_new (FALSE, TRUE, sizeof (MatrixStackEntry));

  g_assert (builder->mv_stack != NULL);

  g_array_set_size (builder->mv_stack, builder->mv_stack->len + 1);
  entry = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 1);

  if (G_LIKELY (builder->mv_stack->len >= 2))
    {
      const MatrixStackEntry *cur;
      GskTransform *t;

      cur = &g_array_index (builder->mv_stack, MatrixStackEntry, builder->mv_stack->len - 2);
      /* Multiply given matrix with current modelview */

      t = gsk_transform_transform (cur->transform, transform);
      t = gsk_transform_translate (t, &(graphene_point_t) { builder->dx * scale, builder->dy * scale });
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
    builder->current_program_state->projection = *projection;

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

  if (builder->current_program_state != NULL &&
      rect_equal (&builder->current_program_state->viewport, viewport))
    return builder->current_program_state->viewport;

  op.op = OP_CHANGE_VIEWPORT;
  op.viewport = *viewport;
  g_array_append_val (builder->render_ops, op);

  if (builder->current_program != NULL)
    builder->current_program_state->viewport = *viewport;

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

  if (builder->render_ops->len > 0)
    {
      RenderOp *last_op = &g_array_index (builder->render_ops, RenderOp, builder->render_ops->len - 1);
      if (last_op->op == OP_CHANGE_RENDER_TARGET)
        {
          last_op->render_target_id = render_target_id;
        }
      else
        {
          op.op = OP_CHANGE_RENDER_TARGET;
          op.render_target_id = render_target_id;
          g_array_append_val (builder->render_ops, op);
        }
    }
  else
    {
      op.op = OP_CHANGE_RENDER_TARGET;
      op.render_target_id = render_target_id;
      g_array_append_val (builder->render_ops, op);
    }

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
    builder->current_program_state->opacity = opacity;

  return prev_opacity;
}

void
ops_set_color (RenderOpBuilder *builder,
               const GdkRGBA   *color)
{
  RenderOp op;

  if (gdk_rgba_equal (color, &builder->current_program_state->color))
    return;

  builder->current_program_state->color = *color;

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
              &builder->current_program_state->color_matrix.matrix,
              sizeof (graphene_matrix_t)) == 0 &&
      memcmp (offset,
              &builder->current_program_state->color_matrix.offset,
              sizeof (graphene_vec4_t)) == 0)
    return;

  builder->current_program_state->color_matrix.matrix = *matrix;
  builder->current_program_state->color_matrix.offset = *offset;

  op.op = OP_CHANGE_COLOR_MATRIX;
  op.color_matrix.matrix = *matrix;
  op.color_matrix.offset = *offset;
  g_array_append_val (builder->render_ops, op);
}

void
ops_set_border (RenderOpBuilder      *builder,
                const GskRoundedRect *outline)
{
  RenderOp op;

  if (memcmp (&builder->current_program_state->border.outline,
              outline, sizeof (GskRoundedRect)) == 0)
    return;

  builder->current_program_state->border.outline = *outline;

  op.op = OP_CHANGE_BORDER;
  op.border.outline = *outline;
  g_array_append_val (builder->render_ops, op);
}

void
ops_set_border_width (RenderOpBuilder *builder,
                      const float     *widths)
{
  RenderOp op;

  if (memcmp (builder->current_program_state->border.widths,
              widths, sizeof (float) * 4) == 0)
    return;

  memcpy (&builder->current_program_state->border.widths,
          widths, sizeof (float) * 4);

  op.op = OP_CHANGE_BORDER_WIDTH;
  op.border.widths[0] = widths[0];
  op.border.widths[1] = widths[1];
  op.border.widths[2] = widths[2];
  op.border.widths[3] = widths[3];

  g_array_append_val (builder->render_ops, op);
}

void
ops_set_border_color (RenderOpBuilder *builder,
                      const GdkRGBA   *color)
{
  RenderOp op;
  op.op = OP_CHANGE_BORDER_COLOR;
  rgba_to_float (color, op.border.color);

  if (memcmp (&op.border.color, &builder->current_program_state->border.color,
              sizeof (float) * 4) == 0)
    return;

  rgba_to_float (color, builder->current_program_state->border.color);

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

void
ops_add (RenderOpBuilder *builder,
         const RenderOp  *op)
{
  g_array_append_val (builder->render_ops, *op);
}
