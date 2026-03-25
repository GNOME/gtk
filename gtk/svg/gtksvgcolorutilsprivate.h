#pragma once

G_BEGIN_DECLS

static inline void
color_apply_color_matrix (const GdkColor          *color,
                          GdkColorState           *color_state,
                          const graphene_matrix_t *matrix,
                          const graphene_vec4_t   *offset,
                          GdkColor                *new_color)
{
  GdkColor c;
  graphene_vec4_t p;
  float v[4];

  gdk_color_convert (&c, color_state, color);
  graphene_vec4_init (&p, c.r, c.g, c.b, c.a);
  graphene_matrix_transform_vec4 (matrix, &p, &p);
  graphene_vec4_add (&p, offset, &p);
  graphene_vec4_to_float (&p, v);
  gdk_color_init (new_color, color_state, v);
  gdk_color_finish (&c);
}

G_END_DECLS
