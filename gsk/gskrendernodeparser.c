
#include "gskrendernodeparserprivate.h"

#include "gskcsstokenizerprivate.h"
#include "gskroundedrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransform.h"

typedef struct
{
  int n_tokens;
  GskCssToken *tokens;

  int pos;
  const GskCssToken *cur;
} Parser;

static void
skip (Parser *p)
{
  p->pos ++;

  g_assert_cmpint (p->pos, <, p->n_tokens);
  p->cur = &p->tokens[p->pos];
}

static const GskCssToken *
lookahead (Parser *p,
           int     lookahead)
{
  g_assert_cmpint (p->pos, <, p->n_tokens - lookahead);

  return &p->tokens[p->pos + lookahead];
}

static void
expect (Parser *p,
        int     expected_type)
{
  if (p->cur->type != expected_type)
    g_error ("Expected token type %d but found %d ('%s')",
             expected_type, p->cur->type, gsk_css_token_to_string (p->cur));
}

static void
expect_skip (Parser *p,
             int     expected_type)
{
  expect (p, expected_type);
  skip (p);
}

static void
expect_skip_ident (Parser     *p,
                   const char *ident)
{
  if (!gsk_css_token_is_ident (p->cur, ident))
    g_error ("Expected ident '%s', but found token %s",
             ident, p->cur->string.string);

  skip (p);
}

static void
parser_init (Parser      *p,
             GskCssToken *tokens,
             int          n_tokens)
{
  p->tokens = tokens;
  p->pos = 0;
  p->cur = &tokens[p->pos];
  p->n_tokens = n_tokens;
}

static GskCssToken *
tokenize (GBytes *bytes,
          int    *n_tokens)
{
  GskCssTokenizer *tokenizer;
  GArray *tokens;
  GskCssToken token;

  tokenizer = gsk_css_tokenizer_new (bytes, NULL, NULL, NULL);
  tokens = g_array_new (FALSE, TRUE, sizeof (GskCssToken));

  for (gsk_css_tokenizer_read_token (tokenizer, &token);
       !gsk_css_token_is (&token, GSK_CSS_TOKEN_EOF);
       gsk_css_tokenizer_read_token (tokenizer, &token))
    {
      g_array_append_val (tokens, token);
    }

  g_array_append_val (tokens, token);

  *n_tokens = (int) tokens->len;

  return (GskCssToken *) g_array_free (tokens, FALSE);
}

static double
number_value (Parser *p)
{
  if (!gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_INTEGER) &&
      !gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_INTEGER) &&
      !gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_NUMBER) &&
      !gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_NUMBER))
    expect (p, GSK_CSS_TOKEN_SIGNED_NUMBER);

  return p->cur->number.number;
}

static void
parse_double4 (Parser *p,
               double *out_values)
{
  int i;

  expect_skip (p, GSK_CSS_TOKEN_OPEN_PARENS);
  out_values[0] = number_value (p);
  skip (p);

  for (i = 0; i < 3; i ++)
    {
      expect_skip (p, GSK_CSS_TOKEN_COMMA);
      out_values[1 + i] = number_value (p);
      skip (p);
    }

  expect_skip (p, GSK_CSS_TOKEN_CLOSE_PARENS);
}

static void
parse_tuple (Parser *p,
             double *out_values)
{
  expect_skip (p, GSK_CSS_TOKEN_OPEN_PARENS);
  out_values[0] = number_value (p);
  skip (p);

  expect_skip (p, GSK_CSS_TOKEN_COMMA);

  out_values[1] = number_value (p);
  skip (p);

  expect_skip (p, GSK_CSS_TOKEN_CLOSE_PARENS);
}

/*
 * The cases we allow are:
 * (x, y, w, h) (w1, h1) (w2, h2) (w3, h3) (w4, h4) for the full rect
 * (x, y, w, h) s1 s2 s3 s4                         for rect + quad corners
 * (x, y, w, h) s                                   for rect + all corners the same size
 * (x, y, w, h)                                     for just the rect with 0-sized corners
 */
static void
parse_rounded_rect (Parser         *p,
                    GskRoundedRect *result)
{
  double rect[4];
  double corner0[2];
  double corner1[2];
  double corner2[2];
  double corner3[2];

  parse_double4 (p, rect);

  if (gsk_css_token_is (p->cur, GSK_CSS_TOKEN_OPEN_PARENS))
    {
      parse_tuple (p, corner0);
      parse_tuple (p, corner1);
      parse_tuple (p, corner2);
      parse_tuple (p, corner3);
    }
  else if (gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_INTEGER) ||
           gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_INTEGER) ||
           gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_NUMBER) ||
           gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      double val = number_value (p);

      corner0[0] = corner0[1] = val;

      skip (p);

      if (gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_INTEGER) ||
          gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_INTEGER) ||
          gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNED_NUMBER) ||
          gsk_css_token_is (p->cur, GSK_CSS_TOKEN_SIGNLESS_NUMBER))
        {
          corner1[0] = corner1[1] = number_value (p);
          skip (p);
          corner2[0] = corner2[1] = number_value (p);
          skip (p);
          corner3[0] = corner3[1] = number_value (p);
          skip (p);
        }
      else
        {
          corner1[0] = corner1[1] = val;
          corner2[0] = corner2[1] = val;
          corner3[0] = corner3[1] = val;
        }
    }
  else
    {
      corner0[0] = corner0[1] = 0.0;
      corner1[0] = corner1[1] = 0.0;
      corner2[0] = corner2[1] = 0.0;
      corner3[0] = corner3[1] = 0.0;
    }

  gsk_rounded_rect_init (result,
                         &GRAPHENE_RECT_INIT (rect[0], rect[1], rect[2], rect[3]),
                         &(graphene_size_t) { corner0[0], corner0[1] },
                         &(graphene_size_t) { corner1[0], corner1[1] },
                         &(graphene_size_t) { corner2[0], corner2[1] },
                         &(graphene_size_t) { corner3[0], corner3[1] });
}

static void
parse_matrix (Parser            *p,
              graphene_matrix_t *matrix)
{
  float vals[16];
  int i;

  expect_skip (p, GSK_CSS_TOKEN_OPEN_PARENS);

  vals[0] = number_value (p);
  skip (p);

  for (i = 1; i < 16; i ++)
    {
      expect_skip (p, GSK_CSS_TOKEN_COMMA);
      vals[i] = number_value (p);
      skip (p);
    }

  expect_skip (p, GSK_CSS_TOKEN_CLOSE_PARENS);

  graphene_matrix_init_from_float (matrix, vals);
}

static double
parse_float_param (Parser     *p,
                   const char *param_name)
{
  double value;

  expect_skip_ident (p, param_name);
  expect_skip (p, GSK_CSS_TOKEN_COLON);
  value = number_value (p);
  skip (p);

  return value;
}

static void
parse_tuple_param (Parser     *p,
                   const char *param_name,
                   double     *values)
{
  expect_skip_ident (p, param_name);
  expect_skip (p, GSK_CSS_TOKEN_COLON);

  parse_tuple (p, values);
}

static void
parse_double4_param (Parser     *p,
                     const char *param_name,
                     double     *values)
{
  expect_skip_ident (p, param_name);
  expect_skip (p, GSK_CSS_TOKEN_COLON);

  parse_double4 (p, values);
}

static void
parse_rounded_rect_param (Parser         *p,
                          const char     *param_name,
                          GskRoundedRect *rect)
{
  expect_skip_ident (p, param_name);
  expect_skip (p, GSK_CSS_TOKEN_COLON);

  parse_rounded_rect (p, rect);
}

static void
parse_matrix_param (Parser            *p,
                    const char        *param_name,
                    graphene_matrix_t *matrix)
{
  expect_skip_ident (p, param_name);
  expect_skip (p, GSK_CSS_TOKEN_COLON);

  parse_matrix (p, matrix);
}

static GskRenderNode *
parse_node (Parser *p)
{
  GskRenderNode *result = NULL;

  if (gsk_css_token_is_ident (p->cur, "color"))
    {
      double color[4];
      double bounds[4];

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_double4_param (p, "bounds", bounds);

      expect_skip_ident (p, "color");
      expect_skip (p, GSK_CSS_TOKEN_COLON);
      parse_double4 (p, color);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_color_node_new (&(GdkRGBA) { color[0], color[1], color[2], color[3] },
                                   &GRAPHENE_RECT_INIT (bounds[0], bounds[1], bounds[2], bounds[3]));
    }
  else if (gsk_css_token_is_ident (p->cur, "opacity"))
    {
      double opacity = 0.0;
      GskRenderNode *child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      expect_skip_ident (p, "opacity");
      expect_skip (p, GSK_CSS_TOKEN_COLON);
      opacity = number_value (p);
      skip (p);

      child = parse_node (p);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_opacity_node_new (child, opacity);
      gsk_render_node_unref (child);
    }
  else if (gsk_css_token_is_ident (p->cur, "container"))
    {
      GPtrArray *children = g_ptr_array_new ();
      guint i;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      while (p->cur->type != GSK_CSS_TOKEN_CLOSE_CURLY)
        g_ptr_array_add (children, parse_node (p));


      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_container_node_new ((GskRenderNode **)children->pdata, children->len);

      for (i = 0; i < children->len; i ++)
        gsk_render_node_unref (g_ptr_array_index (children, i));

      g_ptr_array_free (children, TRUE);
    }
  else if (gsk_css_token_is_ident (p->cur, "outset_shadow"))
    {
      GskRoundedRect outline;
      double color[4];
      float dx, dy, spread, blur_radius;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      parse_rounded_rect_param (p, "outline", &outline);

      parse_double4_param (p, "color", color);
      dx = parse_float_param (p, "dx");
      dy = parse_float_param (p, "dy");
      spread = parse_float_param (p, "spread");
      blur_radius = parse_float_param (p, "blur_radius");

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_outset_shadow_node_new (&outline,
                                           &(GdkRGBA) { color[0], color[1], color[2], color[3] },
                                           dx, dy,
                                           spread,
                                           blur_radius);
    }
  else if (gsk_css_token_is_ident (p->cur, "cross_fade"))
    {
      double progress;
      GskRenderNode *start_child;
      GskRenderNode *end_child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      progress = parse_float_param (p, "progress");
      start_child = parse_node (p);
      end_child = parse_node (p);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_cross_fade_node_new (start_child, end_child, progress);

      gsk_render_node_unref (start_child);
      gsk_render_node_unref (end_child);
    }
  else if (gsk_css_token_is_ident (p->cur, "clip"))
    {
      double clip_rect[4];
      GskRenderNode *child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_double4_param (p, "clip", clip_rect);
      child = parse_node (p);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_clip_node_new (child,
                                  &GRAPHENE_RECT_INIT (
                                    clip_rect[0], clip_rect[1],
                                    clip_rect[2], clip_rect[3]
                                  ));

      gsk_render_node_unref (child);
    }
  else if (gsk_css_token_is_ident (p->cur, "rounded_clip"))
    {
      GskRoundedRect clip_rect;
      GskRenderNode *child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_rounded_rect_param (p, "clip", &clip_rect);
      child = parse_node (p);


      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_rounded_clip_node_new (child, &clip_rect);

      gsk_render_node_unref (child);
    }
  else if (gsk_css_token_is_ident (p->cur, "linear_gradient"))
    {
      GArray *stops = g_array_new (FALSE, TRUE, sizeof (GskColorStop));
      double bounds[4];
      double start[2];
      double end[2];

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      parse_double4_param (p, "bounds", bounds);
      parse_tuple_param (p, "start", start);
      parse_tuple_param (p, "end", end);

      expect_skip_ident (p, "stops");
      expect_skip (p, GSK_CSS_TOKEN_COLON);
      while (p->cur->type == GSK_CSS_TOKEN_OPEN_PARENS)
        {
          GskColorStop stop;
          double color[4];

          skip (p);
          stop.offset = number_value (p);
          skip (p);
          expect_skip (p, GSK_CSS_TOKEN_COMMA);
          parse_double4 (p, color);
          expect_skip (p, GSK_CSS_TOKEN_CLOSE_PARENS);

          stop.color = (GdkRGBA) { color[0], color[1], color[2], color[3] };
          g_array_append_val (stops, stop);
        }

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_linear_gradient_node_new (&GRAPHENE_RECT_INIT (
                                               bounds[0], bounds[1],
                                               bounds[2], bounds[3]
                                             ),
                                             &(graphene_point_t) { start[0], start[1] },
                                             &(graphene_point_t) { end[0], end[1] },
                                             (GskColorStop *)stops->data,
                                             stops->len);
      g_array_free (stops, TRUE);
    }
  else if (gsk_css_token_is_ident (p->cur, "transform"))
    {
      GskTransform *transform;
      GskRenderNode *child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      expect_skip_ident (p, "transform");
      expect_skip (p, GSK_CSS_TOKEN_COLON);

      if (p->cur->type == GSK_CSS_TOKEN_OPEN_PARENS)
        {
          graphene_matrix_t matrix;
          parse_matrix (p, &matrix);

          transform = gsk_transform_matrix (NULL, &matrix);
        }
      else
        {
          expect (p, GSK_CSS_TOKEN_IDENT);

          for (transform = NULL;;)
            {
              /* Transform name */
              expect (p, GSK_CSS_TOKEN_IDENT);

              if (lookahead (p, 1)->type == GSK_CSS_TOKEN_OPEN_CURLY) /* Start of child node */
                break;

              if (gsk_css_token_is_ident (p->cur, "translate"))
                {
                  double offset[2];
                  skip (p);
                  parse_tuple (p, offset);
                  transform = gsk_transform_translate (transform,
                                                       &(graphene_point_t) { offset[0], offset[1] });

                }
              else
                {
                  g_error ("Unknown transform type: %s", gsk_css_token_to_string (p->cur));
                }
            }
        }

      child = parse_node (p);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_transform_node_new (child, transform);

      gsk_transform_unref (transform);
      gsk_render_node_unref (child);
    }
  else if (gsk_css_token_is_ident (p->cur, "color_matrix"))
    {
      double offset_values[4];
      graphene_matrix_t matrix;
      graphene_vec4_t offset;
      GskRenderNode *child;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_matrix_param (p, "matrix", &matrix);
      parse_double4_param (p, "offset", offset_values);

      graphene_vec4_init (&offset,
                          offset_values[0],
                          offset_values[1],
                          offset_values[2],
                          offset_values[3]);

      child = parse_node (p);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_color_matrix_node_new (child, &matrix, &offset);

      gsk_render_node_unref (child);
    }
  else if (gsk_css_token_is_ident (p->cur, "texture"))
    {
      G_GNUC_UNUSED guchar *texture_data;
      gsize texture_data_len;
      GdkTexture *texture;
      double bounds[4];

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_double4_param (p, "bounds", bounds);

      expect_skip (p, GSK_CSS_TOKEN_IDENT);
      expect_skip (p, GSK_CSS_TOKEN_COLON);
      expect (p, GSK_CSS_TOKEN_STRING);

      texture_data = g_base64_decode (p->cur->string.string, &texture_data_len);
      guchar data[] = {1, 0, 0, 1, 0, 0,
                       0, 0, 1, 0, 0, 1};
      GBytes *b = g_bytes_new_static (data, 12);

      /* TODO: :( */
      texture = gdk_memory_texture_new (2, 2, GDK_MEMORY_R8G8B8,
                                        b, 6);

      expect_skip (p, GSK_CSS_TOKEN_STRING);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_texture_node_new (texture,
                                     &GRAPHENE_RECT_INIT (
                                       bounds[0], bounds[1],
                                       bounds[2], bounds[3]
                                     ));
    }
  else if (gsk_css_token_is_ident (p->cur, "inset_shadow"))
    {
      GskRoundedRect outline;
      double color[4];
      float dx, dy, spread, blur_radius;

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);
      parse_rounded_rect_param (p, "outline", &outline);

      parse_double4_param (p, "color", color);
      dx = parse_float_param (p, "dx");
      dy = parse_float_param (p, "dy");
      spread = parse_float_param (p, "spread");
      blur_radius = parse_float_param (p, "blur_radius");

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_inset_shadow_node_new (&outline,
                                          &(GdkRGBA) { color[0], color[1], color[2], color[3] },
                                          dx, dy,
                                          spread,
                                          blur_radius);
    }
  else if (gsk_css_token_is_ident (p->cur, "border"))
    {
      GskRoundedRect outline;
      double widths[4];
      double colors[4][4];

      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      parse_rounded_rect_param (p, "outline", &outline);
      parse_double4_param (p, "widths", widths);

      expect_skip_ident (p, "colors");
      expect_skip (p, GSK_CSS_TOKEN_COLON);

      parse_double4 (p, colors[0]);
      parse_double4 (p, colors[1]);
      parse_double4 (p, colors[2]);
      parse_double4 (p, colors[3]);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);
      result = gsk_border_node_new (&outline,
                                    (float[4]) { widths[0], widths[1], widths[2], widths[3] },
                                    (GdkRGBA[4]) {
                                      (GdkRGBA) { colors[0][0], colors[0][1], colors[0][2], colors[0][3] },
                                      (GdkRGBA) { colors[1][0], colors[1][1], colors[1][2], colors[1][3] },
                                      (GdkRGBA) { colors[2][0], colors[2][1], colors[2][2], colors[2][3] },
                                      (GdkRGBA) { colors[3][0], colors[3][1], colors[3][2], colors[3][3] },
                                    });
    }
  else if (gsk_css_token_is_ident (p->cur, "text"))
    {
      skip (p);
      expect_skip (p, GSK_CSS_TOKEN_OPEN_CURLY);

      expect_skip (p, GSK_CSS_TOKEN_CLOSE_CURLY);

      result = gsk_color_node_new (
                                   &(GdkRGBA) { 0, 1, 0, 1 },
                                   &GRAPHENE_RECT_INIT (0, 0, 0, 0));
    }
  else
    {
      g_error ("Unknown render node type: %s", gsk_css_token_to_string (p->cur));
    }

  return result;
}

/**
 * All errors are fatal.
 */
GskRenderNode *
gsk_render_node_deserialize_from_bytes (GBytes *bytes)
{
  GskRenderNode *root = NULL;
  GskCssToken *tokens;
  int n_tokens;
  Parser parser;

  tokens = tokenize (bytes, &n_tokens);

  parser_init (&parser, tokens, n_tokens);
  root = parse_node (&parser);

  g_free (tokens);

  return root;
}


typedef struct
{
  int indentation_level;
  GString *str;
} Printer;

static void
printer_init (Printer *self)
{
  self->indentation_level = 0;
  self->str = g_string_new (NULL);
}

#define IDENT_LEVEL 2 /* Spaces per level */
static void
_indent (Printer *self)
{
  if (self->indentation_level > 0)
    g_string_append_printf (self->str, "%*s", self->indentation_level * IDENT_LEVEL, " ");
}
#undef IDENT

static void
start_node (Printer    *self,
            const char *node_name)
{
  _indent (self);
  g_string_append_printf (self->str, "%s {\n", node_name);
  self->indentation_level ++;
}

static void
end_node (Printer *self)
{
  self->indentation_level --;
  _indent (self);
  g_string_append (self->str, "}\n");
}

static void
append_rect (GString               *str,
             const graphene_rect_t *r)
{
  g_string_append_printf (str, "(%f, %f, %f, %f)",
                          r->origin.x,
                          r->origin.y,
                          r->size.width,
                          r->size.height);
}

static void
append_rounded_rect (GString              *str,
                     const GskRoundedRect *r)
{
  append_rect (str, &r->bounds);

  if (!gsk_rounded_rect_is_rectilinear (r))
    {
      g_string_append_printf (str, " (%f, %f) (%f, %f) (%f, %f) (%f, %f)",
                              r->corner[0].width,
                              r->corner[0].height,
                              r->corner[1].width,
                              r->corner[1].height,
                              r->corner[2].width,
                              r->corner[2].height,
                              r->corner[3].width,
                              r->corner[3].height);
    }
}

static void
append_rgba (GString         *str,
             const GdkRGBA   *rgba)
{
  g_string_append_printf (str, "(%f, %f, %f, %f)",
                          rgba->red,
                          rgba->green,
                          rgba->blue,
                          rgba->alpha);
}

static void
append_point (GString                *str,
              const graphene_point_t *p)
{
  g_string_append_printf (str, "(%f, %f)", p->x, p->y);
}

static void
append_matrix (GString                 *str,
               const graphene_matrix_t *m)
{
  float v[16];

  graphene_matrix_to_float (m, v);

  g_string_append_printf (str, "(%f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f, %f)",
                          v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7],
                          v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
}

static void
append_vec4 (GString               *str,
             const graphene_vec4_t *v)
{
  g_string_append_printf (str, "(%f, %f, %f, %f)",
                          graphene_vec4_get_x (v),
                          graphene_vec4_get_y (v),
                          graphene_vec4_get_z (v),
                          graphene_vec4_get_w (v));
}

static void
append_float_param (Printer    *p,
                    const char *param_name,
                    float       value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = %f\n", param_name, value);
}

static void
append_rgba_param (Printer       *p,
                   const char    *param_name,
                   const GdkRGBA *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_rgba (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
append_rect_param (Printer               *p,
                   const char            *param_name,
                   const graphene_rect_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_rect (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
append_rounded_rect_param (Printer              *p,
                           const char           *param_name,
                           const GskRoundedRect *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_rounded_rect (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
append_point_param (Printer                *p,
                    const char             *param_name,
                    const graphene_point_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_point (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
append_vec4_param (Printer               *p,
                   const char            *param_name,
                   const graphene_vec4_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_vec4 (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
append_matrix_param (Printer                 *p,
                     const char              *param_name,
                     const graphene_matrix_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s = ", param_name);
  append_matrix (p->str, value);
  g_string_append_c (p->str, '\n');
}

static void
render_node_print (Printer       *p,
                   GskRenderNode *node)
{
  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_CONTAINER_NODE:
      {
        guint i;

        start_node (p, "container");
        for (i = 0; i < gsk_container_node_get_n_children (node); i ++)
          {
            GskRenderNode *child = gsk_container_node_get_child (node, i);

            render_node_print (p, child);
          }
        end_node (p);
      }
    break;

    case GSK_COLOR_NODE:
      {
        start_node (p, "color");
        append_rect_param (p, "bounds", &node->bounds);
        append_rgba_param (p, "color", gsk_color_node_peek_color (node));
        end_node (p);
      }
    break;

    case GSK_CROSS_FADE_NODE:
      {
        start_node (p, "cross_fade");

        append_float_param (p, "progress", gsk_cross_fade_node_get_progress (node));
        render_node_print (p, gsk_cross_fade_node_get_start_child (node));
        render_node_print (p, gsk_cross_fade_node_get_end_child (node));

        end_node (p);
      }
    break;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        int i;

        start_node (p, "linear_gradient");

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "start", gsk_linear_gradient_node_peek_start (node));
        append_point_param (p, "end", gsk_linear_gradient_node_peek_end (node));

        _indent (p);
        g_string_append (p->str, "stops =");
        for (i = 0; i < gsk_linear_gradient_node_get_n_color_stops (node); i ++)
          {
            const GskColorStop *stop = gsk_linear_gradient_node_peek_color_stops (node) + i;

            g_string_append_printf (p->str, " (%f, ", stop->offset);
            append_rgba (p->str, &stop->color);
            g_string_append_c (p->str, ')');
          }
        g_string_append (p->str, "\n");

        end_node (p);
      }
    break;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
      {
        g_error ("Add public api to access the repeating linear gradient node data");
      }
    break;

    case GSK_OPACITY_NODE:
      {
        start_node (p, "opacity");

        append_float_param (p, "opacity", gsk_opacity_node_get_opacity (node));
        render_node_print (p, gsk_opacity_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        start_node (p, "outset_shadow");

        append_rounded_rect_param (p, "outline", gsk_outset_shadow_node_peek_outline (node));
        append_rgba_param (p, "color", gsk_outset_shadow_node_peek_color (node));
        append_float_param (p, "dx", gsk_outset_shadow_node_get_dx (node));
        append_float_param (p, "dy", gsk_outset_shadow_node_get_dy (node));
        append_float_param (p, "spread", gsk_outset_shadow_node_get_spread (node));
        append_float_param (p, "blur_radius", gsk_outset_shadow_node_get_blur_radius (node));

        end_node (p);
      }
    break;

    case GSK_CLIP_NODE:
      {
        start_node (p, "clip");

        append_rect_param (p, "clip", gsk_clip_node_peek_clip (node));
        render_node_print (p, gsk_clip_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        start_node (p, "rounded_clip");

        append_rounded_rect_param (p, "clip", gsk_rounded_clip_node_peek_clip (node));
        render_node_print (p, gsk_rounded_clip_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_TRANSFORM_NODE:
      {
        graphene_matrix_t matrix;

        start_node (p, "transform");

        gsk_transform_to_matrix (gsk_transform_node_get_transform (node), &matrix);
        append_matrix_param (p, "transform", &matrix);
        render_node_print (p, gsk_transform_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_COLOR_MATRIX_NODE:
      {
        start_node (p, "color_matrix");

        append_matrix_param (p, "matrix", gsk_color_matrix_node_peek_color_matrix (node));
        append_vec4_param (p, "offset", gsk_color_matrix_node_peek_color_offset (node));
        render_node_print (p, gsk_color_matrix_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_BORDER_NODE:
      {
        start_node (p, "border");

        append_rounded_rect_param (p, "outline", gsk_border_node_peek_outline (node));

        _indent (p);
        g_string_append (p->str, "widths = (");
        g_string_append_printf (p->str, "%f, ", gsk_border_node_peek_widths (node)[0]);
        g_string_append_printf (p->str, "%f, ", gsk_border_node_peek_widths (node)[1]);
        g_string_append_printf (p->str, "%f, ", gsk_border_node_peek_widths (node)[2]);
        g_string_append_printf (p->str, "%f", gsk_border_node_peek_widths (node)[3]);
        g_string_append (p->str, ")\n");

        _indent (p);
        g_string_append (p->str, "colors = ");
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[0]);
        g_string_append (p->str, " ");
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[1]);
        g_string_append (p->str, " ");
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[2]);
        g_string_append (p->str, " ");
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[3]);
        g_string_append (p->str, "\n");

        end_node (p);
      }
    break;

    case GSK_SHADOW_NODE:
      {
        int i;

        start_node (p, "shadow");

        _indent (p);
        g_string_append (p->str, "shadows = ");
        for (i = 0; i < gsk_shadow_node_get_n_shadows (node); i ++)
          {
            const GskShadow *s = gsk_shadow_node_peek_shadow (node, i);

            g_string_append_printf (p->str, "(%f, (%f, %f), ",
                                    s->radius, s->dx, s->dy);
            append_rgba (p->str, &s->color);
            g_string_append (p->str, ", ");
            /* TODO: One too many commas */
          }

        end_node (p);
      }
    break;

    case GSK_INSET_SHADOW_NODE:
      {
        start_node (p, "inset_shadow");

        append_rounded_rect_param (p, "outline", gsk_inset_shadow_node_peek_outline (node));
        append_rgba_param (p, "color", gsk_inset_shadow_node_peek_color (node));
        append_float_param (p, "dx", gsk_inset_shadow_node_get_dx (node));
        append_float_param (p, "dy", gsk_inset_shadow_node_get_dy (node));
        append_float_param (p, "spread", gsk_inset_shadow_node_get_spread (node));
        append_float_param (p, "blur_radius", gsk_inset_shadow_node_get_blur_radius (node));

        end_node (p);
      }
    break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        int stride;
        int len;
        guchar *data;
        char *b64;

        start_node (p, "texture");
        append_rect_param (p, "bounds", &node->bounds);

        stride = 4 * gdk_texture_get_width (texture);
        len = sizeof (guchar) * stride * gdk_texture_get_height (texture);
        data = g_malloc (len);
        gdk_texture_download (texture, data, stride);

        b64 = g_base64_encode (data, len);

        _indent (p);
        g_string_append_printf (p->str, "data = \"%s\"\n", b64);
        end_node (p);

        g_free (b64);
        g_free (data);
      }
    break;

    case GSK_CAIRO_NODE:
      {

        /*start_node (p, "cairo");*/
        /*append_rect_param (p, "bounds", &node->bounds);*/
        /*g_string_append (p->str, "?????");*/
        /*end_node (p);*/
      }
    break;

    case GSK_TEXT_NODE:
      {
        start_node (p, "text");

        _indent (p);
        g_string_append_printf (p->str, "font = \"%s\"\n",
                                pango_font_description_to_string (pango_font_describe (
                                     (PangoFont *)gsk_text_node_peek_font (node))));

        append_float_param (p, "x", gsk_text_node_get_x (node));
        append_float_param (p, "y", gsk_text_node_get_y (node));
        append_rgba_param (p, "color", gsk_text_node_peek_color (node));

        end_node (p);
      }
    break;

    case GSK_DEBUG_NODE:
      {
        start_node (p, "debug");

        _indent (p);
        /* TODO: We potentially need to escape certain characters in the message */
        g_string_append_printf (p->str, "message = \"%s\"\n", gsk_debug_node_get_message (node));

        render_node_print (p, gsk_debug_node_get_child (node));
        end_node (p);
      }
    break;

    case GSK_BLUR_NODE:
      {
        start_node (p, "blur");

        append_float_param (p, "radius", gsk_blur_node_get_radius (node));
        render_node_print (p, gsk_blur_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_REPEAT_NODE:
      {
        start_node (p, "repeat");
        append_rect_param (p, "bounds", &node->bounds);
        append_rect_param (p, "child_bounds", gsk_repeat_node_peek_child_bounds (node));

        render_node_print (p, gsk_repeat_node_get_child (node));

        end_node (p);
      }
    break;

    case GSK_BLEND_NODE:
      {
        start_node (p, "blend");

        _indent (p);
        /* TODO: (de)serialize enums! */
        g_string_append_printf (p->str, "mode = %d\n", gsk_blend_node_get_blend_mode (node));
        render_node_print (p, gsk_blend_node_get_bottom_child (node));
        render_node_print (p, gsk_blend_node_get_top_child (node));

        end_node (p);
      }
    break;

    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    default:
      g_error ("Unhandled node: %s", node->node_class->type_name);
    }
}

char *
gsk_render_node_serialize_to_string (GskRenderNode *root)
{
  Printer p;

  printer_init (&p);
  render_node_print (&p, root);

  return g_string_free (p.str, FALSE);
}
