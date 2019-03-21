
#include "gskrendernodeparserprivate.h"

#include "gskcssparserprivate.h"
#include "gskroundedrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransformprivate.h"

typedef struct _Declaration Declaration;

struct _Declaration
{
  const char *name;
  gboolean (* parse_func) (GskCssParser *parser, gpointer result);
  gpointer result;
};

static gboolean
parse_color_channel_value (GskCssParser *parser,
                           double       *value,
                           gboolean      is_percentage)
{
  if (is_percentage)
    {
      if (!gsk_css_parser_consume_percentage (parser, value))
        return FALSE;

      *value = CLAMP (*value, 0.0, 100.0) / 100.0;
      return TRUE;
    }
  else
    {
      if (!gsk_css_parser_consume_number (parser, value))
        return FALSE;

      *value = CLAMP (*value, 0.0, 255.0) / 255.0;
      return TRUE;
    }
}

static guint
parse_color_channel (GskCssParser *parser,
                     guint         arg,
                     gpointer      data)
{
  GdkRGBA *rgba = data;

  if (arg == 0)
    {
      /* We abuse rgba->alpha to store if we use percentages or numbers */
      if (gsk_css_token_is (gsk_css_parser_get_token (parser), GSK_CSS_TOKEN_PERCENTAGE))
        rgba->alpha = 1.0;
      else
        rgba->alpha = 0.0;

      if (!parse_color_channel_value (parser, &rgba->red, rgba->alpha != 0.0))
        return 0;
    }
  else if (arg == 1)
    {
      if (!parse_color_channel_value (parser, &rgba->green, rgba->alpha != 0.0))
        return 0;
    }
  else if (arg == 2)
    {
      if (!parse_color_channel_value (parser, &rgba->blue, rgba->alpha != 0.0))
        return 0;
    }
  else if (arg == 3)
    {
      if (!gsk_css_parser_consume_number (parser, &rgba->alpha))
        return FALSE;

      rgba->alpha = CLAMP (rgba->alpha, 0.0, 1.0);
    }
  else
    {
      g_assert_not_reached ();
    }

  return 1;
}

static gboolean
rgba_init_chars (GdkRGBA    *rgba,
                 const char  s[8])
{
  guint i;

  for (i = 0; i < 8; i++)
    {
      if (!g_ascii_isxdigit (s[i]))
        return FALSE;
    }

  rgba->red =   (g_ascii_xdigit_value (s[0]) * 16 + g_ascii_xdigit_value (s[1])) / 255.0;
  rgba->green = (g_ascii_xdigit_value (s[2]) * 16 + g_ascii_xdigit_value (s[3])) / 255.0;
  rgba->blue =  (g_ascii_xdigit_value (s[4]) * 16 + g_ascii_xdigit_value (s[5])) / 255.0;
  rgba->alpha = (g_ascii_xdigit_value (s[6]) * 16 + g_ascii_xdigit_value (s[7])) / 255.0;

  return TRUE;
}

static gboolean
gsk_rgba_parse (GskCssParser *parser,
                GdkRGBA      *rgba)
{
  const GskCssToken *token;

  token = gsk_css_parser_get_token (parser);
  if (gsk_css_token_is_function (token, "rgb"))
    {
      if (!gsk_css_parser_consume_function (parser, 3, 3, parse_color_channel, rgba))
        return FALSE;

      rgba->alpha = 1.0;
      return TRUE;
    }
  else if (gsk_css_token_is_function (token, "rgba"))
    {
      return gsk_css_parser_consume_function (parser, 4, 4, parse_color_channel, rgba);
    }
  else if (gsk_css_token_is (token, GSK_CSS_TOKEN_HASH_ID) ||
           gsk_css_token_is (token, GSK_CSS_TOKEN_HASH_UNRESTRICTED))
    {
      const char *s = token->string.string;

      switch (strlen (s))
        {
          case 3:
            if (rgba_init_chars (rgba, (char[8]) {s[0], s[0], s[1], s[1], s[2], s[2], 'F', 'F' }))
              return TRUE;
            break;

          case 4:
            if (rgba_init_chars (rgba, (char[8]) {s[0], s[0], s[1], s[1], s[2], s[2], s[3], s[3] }))
              return TRUE;
            break;

          case 6:
            if (rgba_init_chars (rgba, (char[8]) {s[0], s[1], s[2], s[3], s[4], s[5], 'F', 'F' }))
              return TRUE;
            break;

          case 8:
            if (rgba_init_chars (rgba, s))
              return TRUE;
            break;

          default:
            break;
        }

      gsk_css_parser_error_value (parser, "Hash code is not a valid hex color.");
      return FALSE;
    }
  else if (gsk_css_token_is (token, GSK_CSS_TOKEN_IDENT))
    {
      if (gsk_css_token_is_ident (token, "transparent"))
        {
          rgba = &(GdkRGBA) { 0, 0, 0, 0 };
        }
      else if (gdk_rgba_parse (rgba, token->string.string))
        {
          /* everything's fine */
        }
      else
        {
          gsk_css_parser_error_value (parser, "\"%s\" is not a known color name.", token->string.string);
          return FALSE;
        }

      gsk_css_parser_consume_token (parser);
      return TRUE;
    }
  else
    {
      gsk_css_parser_error_syntax (parser, "Expected a valid color.");
      return FALSE;
    }
}

static gboolean
parse_semicolon (GskCssParser *parser)
{
  const GskCssToken *token;

  token = gsk_css_parser_get_token (parser);
  if (gsk_css_token_is (token, GSK_CSS_TOKEN_EOF))
    {
      gsk_css_parser_warn_syntax (parser, "No ';' at end of block");
      return TRUE;
    }
  else if (!gsk_css_token_is (token, GSK_CSS_TOKEN_SEMICOLON))
    {
      gsk_css_parser_error_syntax (parser, "Expected ';' at end of statement");
      return FALSE;
    }

  gsk_css_parser_consume_token (parser);
  return TRUE;
}

static gboolean
parse_rect_without_semicolon (GskCssParser    *parser,
                              graphene_rect_t *out_rect)
{
  double numbers[4];
  
  if (!gsk_css_parser_consume_number (parser, &numbers[0]) ||
      !gsk_css_parser_consume_number (parser, &numbers[1]) ||
      !gsk_css_parser_consume_number (parser, &numbers[2]) ||
      !gsk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_rect_init (out_rect, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static gboolean
parse_rect (GskCssParser *parser,
            gpointer      out_rect)
{
  graphene_rect_t r;

  if (!parse_rect_without_semicolon (parser, &r) ||
      !parse_semicolon (parser))
    return FALSE;

  graphene_rect_init_from_rect (out_rect, &r);
  return TRUE;
}

static gboolean
parse_rounded_rect (GskCssParser *parser,
                    gpointer      out_rect)
{
  const GskCssToken *token;
  graphene_rect_t r;
  graphene_size_t corners[4];
  double d;
  guint i;

  if (!parse_rect_without_semicolon (parser, &r))
    return FALSE;

  token = gsk_css_parser_get_token (parser);
  if (!gsk_css_token_is_delim (token, '/'))
    {
      if (!parse_semicolon (parser))
        return FALSE;
      gsk_rounded_rect_init_from_rect (out_rect, &r, 0);
      return TRUE;
    }
  gsk_css_parser_consume_token (parser);

  for (i = 0; i < 4; i++)
    {
      token = gsk_css_parser_get_token (parser);
      if (gsk_css_token_is (token, GSK_CSS_TOKEN_SEMICOLON) ||
          gsk_css_token_is (token, GSK_CSS_TOKEN_EOF))
        break;
      if (!gsk_css_parser_consume_number (parser, &d))
        return FALSE;
      corners[i].width = d;
    }

  if (i == 0)
    {
      gsk_css_parser_error_syntax (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases
   */
  for (; i < 4; i++)
    corners[i].width = corners[(i - 1) >> 1].width;

  token = gsk_css_parser_get_token (parser);
  if (gsk_css_token_is_delim (token, '/'))
    {
      gsk_css_parser_consume_token (parser);

      for (i = 0; i < 4; i++)
        {
          token = gsk_css_parser_get_token (parser);
          if (gsk_css_token_is (token, GSK_CSS_TOKEN_SEMICOLON) ||
              gsk_css_token_is (token, GSK_CSS_TOKEN_EOF))
            break;
          if (!gsk_css_parser_consume_number (parser, &d))
            return FALSE;
          corners[i].height = d;
        }

      if (i == 0)
        {
          gsk_css_parser_error_syntax (parser, "Expected a number");
          return FALSE;
        }

      for (; i < 4; i++)
        corners[i].height = corners[(i - 1) >> 1].height;
    }
  else
    {
      for (i = 0; i < 4; i++)
        corners[i].height = corners[i].width;
    }

  if (!parse_semicolon (parser))
    return FALSE;

  gsk_rounded_rect_init (out_rect, &r, &corners[0], &corners[1], &corners[2], &corners[3]);

  return TRUE;
}

static gboolean
parse_color (GskCssParser *parser,
             gpointer      out_color)
{
  GdkRGBA color;

  if (!gsk_rgba_parse (parser, &color) ||
      !parse_semicolon (parser))
    return FALSE;

  *(GdkRGBA *) out_color = color;

  return TRUE;
}

static gboolean
parse_double (GskCssParser *parser,
              gpointer      out_double)
{
  double d;

  if (!gsk_css_parser_consume_number (parser, &d) ||
      !parse_semicolon (parser))
    return FALSE;

  *(double *) out_double = d;

  return TRUE;
}

static gboolean
parse_point (GskCssParser *parser,
             gpointer      out_point)
{
  double x, y;

  if (!gsk_css_parser_consume_number (parser, &x) ||
      !gsk_css_parser_consume_number (parser, &y) ||
      !parse_semicolon (parser))
    return FALSE;

  graphene_point_init (out_point, x, y);

  return TRUE;
}

static gboolean
parse_transform (GskCssParser *parser,
                 gpointer      out_transform)
{
  GskTransform *transform;

  if (!gsk_transform_parse (parser, &transform) ||
      !parse_semicolon (parser))
    {
      gsk_transform_unref (transform);
      return FALSE;
    }

  gsk_transform_unref (*(GskTransform **) out_transform);
  *(GskTransform **) out_transform = transform;

  return TRUE;
}

static gboolean
parse_string (GskCssParser *parser,
              gpointer      out_string)
{
  const GskCssToken *token;
  char *s;

  token = gsk_css_parser_get_token (parser);
  if (!gsk_css_token_is (token, GSK_CSS_TOKEN_STRING))
    return FALSE;

  s = g_strdup (token->string.string);
  gsk_css_parser_consume_token (parser);

  if (!parse_semicolon (parser))
    {
      g_free (s);
      return FALSE;
    }

  g_free (*(char **) out_string);
  *(char **) out_string = s;

  return TRUE;
}

static gboolean
parse_stops (GskCssParser *parser,
             gpointer      out_stops)
{
  GArray *stops;
  GskColorStop stop;

  stops = g_array_new (FALSE, FALSE, sizeof (GskColorStop));

  do
    {
      if (!gsk_css_parser_consume_number (parser, &stop.offset) ||
          !gsk_rgba_parse (parser, &stop.color))
        { /* do nothing */ }
      else if (stops->len == 0 && stop.offset < 0)
        gsk_css_parser_error_value (parser, "Color stop offset must be >= 0");
      else if (stops->len > 0 && stop.offset < g_array_index (stops, GskColorStop, stops->len - 1).offset)
        gsk_css_parser_error_value (parser, "Color stop offset must be >= previous value");
      else if (stop.offset > 1)
        gsk_css_parser_error_value (parser, "Color stop offset must be <= 1");
      else
        {
          g_array_append_val (stops, stop);
          continue;
        }

      g_array_free (stops, TRUE);
      return FALSE;
    }
  while (gsk_css_parser_consume_if (parser, GSK_CSS_TOKEN_COMMA));

  if (stops->len < 2)
    {
      gsk_css_parser_error_value (parser, "At least 2 color stops need to be specified");
      g_array_free (stops, TRUE);
      return FALSE;
    }

  if (*(GArray **) out_stops)
    g_array_free (*(GArray **) out_stops, TRUE);
  *(GArray **) out_stops = stops;

  return parse_semicolon (parser);
}

static gboolean
parse_node (GskCssParser *parser, gpointer out_node);

static GskRenderNode *
parse_container_node (GskCssParser *parser)
{
  GskRenderNode *node;
  GPtrArray *nodes;
  const GskCssToken *token;
  
  nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  for (token = gsk_css_parser_get_token (parser);
       !gsk_css_token_is (token, GSK_CSS_TOKEN_EOF);
       token = gsk_css_parser_get_token (parser))
    {
      node = NULL;
      if (parse_node (parser, &node))
        {
          g_ptr_array_add (nodes, node);
        }
      else
        {
          gsk_css_parser_skip_until (parser, GSK_CSS_TOKEN_OPEN_CURLY);
          gsk_css_parser_skip (parser);
        }
    }

  node = gsk_container_node_new ((GskRenderNode **) nodes->pdata, nodes->len);

  g_ptr_array_unref (nodes);

  return node;
}

static void
parse_declarations_sync (GskCssParser *parser)
{
  const GskCssToken *token;

  for (token = gsk_css_parser_get_token (parser);
       !gsk_css_token_is (token, GSK_CSS_TOKEN_EOF);
       token = gsk_css_parser_get_token (parser))
    {
      if (gsk_css_token_is (token, GSK_CSS_TOKEN_SEMICOLON) ||
          gsk_css_token_is (token, GSK_CSS_TOKEN_OPEN_CURLY))
        {
          gsk_css_parser_skip (parser);
          break;
        }
      gsk_css_parser_skip (parser);
    }
}

static guint
parse_declarations (GskCssParser      *parser,
                    const Declaration *declarations,
                    guint              n_declarations)
{
  guint parsed = 0;
  guint i;
  const GskCssToken *token;

  g_assert (n_declarations < 8 * sizeof (guint));

  for (token = gsk_css_parser_get_token (parser);
       !gsk_css_token_is (token, GSK_CSS_TOKEN_EOF);
       token = gsk_css_parser_get_token (parser))
    {
      for (i = 0; i < n_declarations; i++)
        {
          if (gsk_css_token_is_ident (token, declarations[i].name))
            {
              gsk_css_parser_consume_token (parser);
              token = gsk_css_parser_get_token (parser);
              if (!gsk_css_token_is (token, GSK_CSS_TOKEN_COLON))
                {
                  gsk_css_parser_error_syntax (parser, "Expected ':' after variable declaration");
                  parse_declarations_sync (parser);
                }
              else
                {
                  gsk_css_parser_consume_token (parser);
                  if (parsed & (1 << i))
                    gsk_css_parser_warn_syntax (parser, "Variable \"%s\" defined multiple times", declarations[i].name);
                  if (declarations[i].parse_func (parser, declarations[i].result))
                    parsed |= (1 << i);
                  else
                    parse_declarations_sync (parser);
                }
              break;
            }
        }
      if (i == n_declarations)
        {
          if (gsk_css_token_is (token, GSK_CSS_TOKEN_IDENT))
            gsk_css_parser_error_syntax (parser, "No variable named \"%s\"", token->string.string);
          else
            gsk_css_parser_error_syntax (parser, "Expected a variable name");
          parse_declarations_sync (parser);
        }
    }

  return parsed;
}

static GskRenderNode *
parse_color_node (GskCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  GdkRGBA color = { 0, 0, 0, 0 };
  const Declaration declarations[] = {
    { "bounds", parse_rect, &bounds },
    { "color", parse_color, &color },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  return gsk_color_node_new (&color, &bounds);
}

static GskRenderNode *
parse_linear_gradient_node (GskCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  graphene_point_t start = GRAPHENE_POINT_INIT (0, 0);
  graphene_point_t end = GRAPHENE_POINT_INIT (0, 0);
  GArray *stops = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, &bounds },
    { "start", parse_point, &start },
    { "end", parse_point, &end },
    { "stops", parse_stops, &stops },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (stops == NULL)
    {
      gsk_css_parser_error_syntax (parser, "No color stops given");
      return NULL;
    }

  result = gsk_linear_gradient_node_new (&bounds, &start, &end, (GskColorStop *) stops->data, stops->len);

  g_array_free (stops, TRUE);

  return result;
}

static GskRenderNode *
parse_inset_shadow_node (GskCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 0, 0);
  GdkRGBA color = { 0, 0, 0, 0 };
  double dx, dy, blur, spread;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, &outline },
    { "color", parse_color, &color },
    { "dx", parse_double, &dx },
    { "dy", parse_double, &dy },
    { "spread", parse_double, &spread },
    { "blur", parse_double, &blur }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  return gsk_inset_shadow_node_new (&outline, &color, dx, dy, spread, blur);
}

static GskRenderNode *
parse_outset_shadow_node (GskCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 0, 0);
  GdkRGBA color = { 0, 0, 0, 0 };
  double dx, dy, blur, spread;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, &outline },
    { "color", parse_color, &color },
    { "dx", parse_double, &dx },
    { "dy", parse_double, &dy },
    { "spread", parse_double, &spread },
    { "blur", parse_double, &blur }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  return gsk_outset_shadow_node_new (&outline, &color, dx, dy, spread, blur);
}

static GskRenderNode *
parse_transform_node (GskCssParser *parser)
{
  GskRenderNode *child = NULL;
  GskTransform *transform = NULL;
  const Declaration declarations[] = {
    { "transform", parse_transform, &transform },
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gsk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      gsk_transform_unref (transform);
      return NULL;
    }
  /* This is very much cheating, isn't it? */
  if (transform == NULL)
    transform = gsk_transform_new ();

  result = gsk_transform_node_new (child, transform);

  gsk_render_node_unref (child);
  gsk_transform_unref (transform);

  return result;
}

static GskRenderNode *
parse_opacity_node (GskCssParser *parser)
{
  GskRenderNode *child = NULL;
  double opacity = 1.0;
  const Declaration declarations[] = {
    { "opacity", parse_double, &opacity },
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gsk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_opacity_node_new (child, opacity);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_cross_fade_node (GskCssParser *parser)
{
  GskRenderNode *start = NULL;
  GskRenderNode *end = NULL;
  double progress = 0.5;
  const Declaration declarations[] = {
    { "progress", parse_double, &progress },
    { "start", parse_node, &start },
    { "end", parse_node, &end },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (start == NULL || end == NULL)
    {
      if (start == NULL)
        gsk_css_parser_error_syntax (parser, "Missing \"start\" property definition");
      if (end == NULL)
        gsk_css_parser_error_syntax (parser, "Missing \"end\" property definition");
      g_clear_pointer (&start, gsk_render_node_unref);
      g_clear_pointer (&end, gsk_render_node_unref);
      return NULL;
    }

  result = gsk_cross_fade_node_new (start, end, progress);

  gsk_render_node_unref (start);
  gsk_render_node_unref (end);

  return result;
}

static GskRenderNode *
parse_clip_node (GskCssParser *parser)
{
  graphene_rect_t clip = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rect, &clip },
    { "child", parse_node, &child },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gsk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  return gsk_clip_node_new (child, &clip);
}

static GskRenderNode *
parse_rounded_clip_node (GskCssParser *parser)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 0, 0);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, &clip },
    { "child", parse_node, &child },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gsk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  return gsk_rounded_clip_node_new (child, &clip);
}

static GskRenderNode *
parse_debug_node (GskCssParser *parser)
{
  char *message = NULL;
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "message", parse_string, &message},
    { "child", parse_node, &child },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gsk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  return gsk_debug_node_new (child, message);
}

static gboolean
parse_node (GskCssParser *parser,
            gpointer      out_node)
{
  static struct {
    const char *name;
    GskRenderNode * (* func) (GskCssParser *);
  } node_parsers[] = {
    { "container", parse_container_node },
    { "color", parse_color_node },
#if 0
    { "cairo", parse_cairo_node },
#endif
    { "linear-gradient", parse_linear_gradient_node },
#if 0
    { "border", parse_border_node },
    { "texture", parse_texture_node },
#endif
    { "inset-shadow", parse_inset_shadow_node },
    { "outset-shadow", parse_outset_shadow_node },
    { "transform", parse_transform_node },
    { "opacity", parse_opacity_node },
#if 0
    { "color-matrix", parse_color-matrix_node },
    { "repeat", parse_repeat_node },
#endif
    { "clip", parse_clip_node },
    { "rounded-clip", parse_rounded_clip_node },
#if 0
    { "shadow", parse_shadow_node },
    { "blend", parse_blend_node },
#endif
    { "cross-fade", parse_cross_fade_node },
#if 0
    { "text", parse_text_node },
    { "blur", parse_blur_node },
#endif
    { "debug", parse_debug_node }
  };
  const GskCssToken *token;
  guint i;
  
  token = gsk_css_parser_get_token (parser);
  if (!gsk_css_token_is (token, GSK_CSS_TOKEN_IDENT))
    {
      gsk_css_parser_error_syntax (parser, "Expected a node name");
      return FALSE;
    }

  for (i = 0; i < G_N_ELEMENTS (node_parsers); i++)
    {
      if (gsk_css_token_is_ident (token, node_parsers[i].name))
        {
          GskRenderNode *node;

          gsk_css_parser_consume_token (parser);
          token = gsk_css_parser_get_token (parser);
          if (!gsk_css_token_is (token, GSK_CSS_TOKEN_OPEN_CURLY))
            {
              gsk_css_parser_error_syntax (parser, "Expected '{' after node name");
              return FALSE;
            }
          gsk_css_parser_start_block (parser);
          node = node_parsers[i].func (parser);
          if (node)
            {
              token = gsk_css_parser_get_token (parser);
              if (!gsk_css_token_is (token, GSK_CSS_TOKEN_EOF))
                gsk_css_parser_error_syntax (parser, "Expected '}' at end of node definition");
              g_clear_pointer ((GskRenderNode **) out_node, gsk_render_node_unref);
              *(GskRenderNode **) out_node = node;
            }
          gsk_css_parser_end_block (parser);

          return node != NULL;
        }
    }

  gsk_css_parser_error_value (parser, "\"%s\" is not a valid node name", token->string.string);
  return FALSE;
}

static void
gsk_render_node_parser_error (GskCssParser         *parser,
                              const GskCssLocation *location,
                              const GskCssToken    *token,
                              const GError         *error,
                              gpointer              user_data)
{
  g_print ("ERROR: %zu:%zu: %s\n",
           location->lines, location->line_chars,
           error->message);
}

/**
 * All errors are fatal.
 */
GskRenderNode *
gsk_render_node_deserialize_from_bytes (GBytes *bytes)
{
  GskRenderNode *root = NULL;
  GskCssParser *parser;

  parser = gsk_css_parser_new (gsk_render_node_parser_error,
                               NULL,
                               NULL);
  gsk_css_parser_add_bytes (parser, bytes);
  root = parse_container_node (parser);

  gsk_css_parser_unref (parser);

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
