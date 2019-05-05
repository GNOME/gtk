
#include "gskrendernodeparserprivate.h"

#include <gdk/gdkrgbaprivate.h>
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssparserprivate.h"
#include "gskroundedrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransformprivate.h"

typedef struct _Declaration Declaration;

struct _Declaration
{
  const char *name;
  gboolean (* parse_func) (GtkCssParser *parser, gpointer result);
  gpointer result;
};

static gboolean
parse_semicolon (GtkCssParser *parser)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (parser);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_warn_syntax (parser, "No ';' at end of block");
      return TRUE;
    }
  else if (!gtk_css_token_is (token, GTK_CSS_TOKEN_SEMICOLON))
    {
      gtk_css_parser_error_syntax (parser, "Expected ';' at end of statement");
      return FALSE;
    }

  gtk_css_parser_consume_token (parser);
  return TRUE;
}

static gboolean
parse_rect_without_semicolon (GtkCssParser    *parser,
                              graphene_rect_t *out_rect)
{
  double numbers[4];

  if (!gtk_css_parser_consume_number (parser, &numbers[0]) ||
      !gtk_css_parser_consume_number (parser, &numbers[1]) ||
      !gtk_css_parser_consume_number (parser, &numbers[2]) ||
      !gtk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_rect_init (out_rect, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static gboolean
parse_rect (GtkCssParser *parser,
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
parse_data (GtkCssParser *parser,
            gpointer      out_data)
{
  const GtkCssToken *token;
  struct {
    guchar *data;
    gsize data_len;
  } *texture_data = out_data;

  token = gtk_css_parser_get_token (parser);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    return FALSE;

  if (!g_str_has_prefix (token->string.string, "data:;base64,"))
    {
      gtk_css_parser_error_value (parser, "Only base64 encoded data is allowed");
      return FALSE;
    }

  texture_data->data = g_base64_decode (token->string.string + strlen ("data:;base64,"),
                                        &texture_data->data_len);

  gtk_css_parser_consume_token (parser);
  if (!parse_semicolon (parser))
    {
      g_free (texture_data->data);
      return FALSE;
    }

  return TRUE;
}

static gboolean
parse_rounded_rect (GtkCssParser *parser,
                    gpointer      out_rect)
{
  graphene_rect_t r;
  graphene_size_t corners[4];
  double d;
  guint i;

  if (!parse_rect_without_semicolon (parser, &r))
    return FALSE;

  if (!gtk_css_parser_try_delim (parser, '/'))
    {
      if (!parse_semicolon (parser))
        return FALSE;
      gsk_rounded_rect_init_from_rect (out_rect, &r, 0);
      return TRUE;
    }

  for (i = 0; i < 4; i++)
    {
      if (!gtk_css_parser_has_number (parser))
        break;
      if (!gtk_css_parser_consume_number (parser, &d))
        return FALSE;
      corners[i].width = d;
    }

  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return FALSE;
    }

  /* The magic (i - 1) >> 1 below makes it take the correct value
   * according to spec. Feel free to check the 4 cases
   */
  for (; i < 4; i++)
    corners[i].width = corners[(i - 1) >> 1].width;

  if (gtk_css_parser_try_delim (parser, '/'))
    {
      gtk_css_parser_consume_token (parser);

      for (i = 0; i < 4; i++)
        {
          if (!gtk_css_parser_has_number (parser))
            break;
          if (!gtk_css_parser_consume_number (parser, &d))
            return FALSE;
          corners[i].height = d;
        }

      if (i == 0)
        {
          gtk_css_parser_error_syntax (parser, "Expected a number");
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
parse_color (GtkCssParser *parser,
             gpointer      out_color)
{
  GdkRGBA color;

  if (!gdk_rgba_parser_parse (parser, &color) ||
      !parse_semicolon (parser))
    return FALSE;

  *(GdkRGBA *) out_color = color;

  return TRUE;
}

static gboolean
parse_double (GtkCssParser *parser,
              gpointer      out_double)
{
  double d;

  if (!gtk_css_parser_consume_number (parser, &d) ||
      !parse_semicolon (parser))
    return FALSE;

  *(double *) out_double = d;

  return TRUE;
}

static gboolean
parse_point (GtkCssParser *parser,
             gpointer      out_point)
{
  double x, y;

  if (!gtk_css_parser_consume_number (parser, &x) ||
      !gtk_css_parser_consume_number (parser, &y) ||
      !parse_semicolon (parser))
    return FALSE;

  graphene_point_init (out_point, x, y);

  return TRUE;
}

static gboolean
parse_transform (GtkCssParser *parser,
                 gpointer      out_transform)
{
  GskTransform *transform;

  if (!gsk_transform_parser_parse (parser, &transform) ||
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
parse_string (GtkCssParser *parser,
              gpointer      out_string)
{
  const GtkCssToken *token;
  char *s;

  token = gtk_css_parser_get_token (parser);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    return FALSE;

  s = g_strdup (token->string.string);
  gtk_css_parser_consume_token (parser);

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
parse_stops (GtkCssParser *parser,
             gpointer      out_stops)
{
  GArray *stops;
  GskColorStop stop;

  stops = g_array_new (FALSE, FALSE, sizeof (GskColorStop));

  for (;;)
    {
      if (!gtk_css_parser_consume_number (parser, &stop.offset))
        goto error;

      if (!gdk_rgba_parser_parse (parser, &stop.color))
        goto error;

      if (stops->len == 0 && stop.offset < 0)
        gtk_css_parser_error_value (parser, "Color stop offset must be >= 0");
      else if (stops->len > 0 && stop.offset < g_array_index (stops, GskColorStop, stops->len - 1).offset)
        gtk_css_parser_error_value (parser, "Color stop offset must be >= previous value");
      else if (stop.offset > 1)
        gtk_css_parser_error_value (parser, "Color stop offset must be <= 1");
      else
        g_array_append_val (stops, stop);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
      else
        break;
  }

  if (stops->len < 2)
    {
      gtk_css_parser_error_value (parser, "At least 2 color stops need to be specified");
      g_array_free (stops, TRUE);
      return FALSE;
    }

  if (*(GArray **) out_stops)
    g_array_free (*(GArray **) out_stops, TRUE);
  *(GArray **) out_stops = stops;

  return parse_semicolon (parser);

error:
  g_array_free (stops, TRUE);
  return FALSE;
}

static gboolean
parse_colors4 (GtkCssParser *parser,
               gpointer      out_colors)
{
  GdkRGBA *colors = (GdkRGBA *)out_colors;
  int i;

  for (i = 0; i < 4; i ++)
    {
      if (!gdk_rgba_parser_parse (parser, &colors[i]))
        return FALSE;
    }

  return parse_semicolon (parser);
}

static gboolean
parse_shadows (GtkCssParser *parser,
               gpointer      out_shadows)
{
  GArray *shadows = out_shadows;

  for (;;)
    {
      GskShadow shadow = { {0, 0, 0, 1}, 0, 0, 0 };
      double dx = 0, dy = 0, radius = 0;

      if (!gdk_rgba_parser_parse (parser, &shadow.color))
        gtk_css_parser_error_value (parser, "Expected shadow color");

      if (!gtk_css_parser_consume_number (parser, &dx))
        gtk_css_parser_error_value (parser, "Expected shadow x offset");

      if (!gtk_css_parser_consume_number (parser, &dy))
        gtk_css_parser_error_value (parser, "Expected shadow x offset");

      if (!gtk_css_parser_consume_number (parser, &radius))
        gtk_css_parser_error_value (parser, "Expected shadow blur radius");

      shadow.dx = dx;
      shadow.dy = dy;
      shadow.radius = radius;

      g_array_append_val (shadows, shadow);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
      else
        break;
    }

  return parse_semicolon (parser);
}

static const struct
{
  GskBlendMode mode;
  const char *name;
} blend_modes[] = {
  { GSK_BLEND_MODE_DEFAULT, "normal" },
  { GSK_BLEND_MODE_MULTIPLY, "multiply" },
  { GSK_BLEND_MODE_SCREEN, "screen" },
  { GSK_BLEND_MODE_OVERLAY, "overlay" },
  { GSK_BLEND_MODE_DARKEN, "darken" },
  { GSK_BLEND_MODE_LIGHTEN, "lighten" },
  { GSK_BLEND_MODE_COLOR_DODGE, "color-dodge" },
  { GSK_BLEND_MODE_COLOR_BURN, "color-burn" },
  { GSK_BLEND_MODE_HARD_LIGHT, "hard-light" },
  { GSK_BLEND_MODE_SOFT_LIGHT, "soft-light" },
  { GSK_BLEND_MODE_DIFFERENCE, "difference" },
  { GSK_BLEND_MODE_EXCLUSION, "exclusion" },
  { GSK_BLEND_MODE_COLOR, "color" },
  { GSK_BLEND_MODE_HUE, "hue" },
  { GSK_BLEND_MODE_SATURATION, "saturation" },
  { GSK_BLEND_MODE_LUMINOSITY, "luminosity" }
};

static gboolean
parse_blend_mode (GtkCssParser *parser,
                  gpointer      out_mode)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (blend_modes); i++)
    {
      if (gtk_css_parser_try_ident (parser, blend_modes[i].name))
        {
          if (!parse_semicolon (parser))
            return FALSE;
          *(GskBlendMode *) out_mode = blend_modes[i].mode;
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
parse_font (GtkCssParser *parser,
            gpointer      out_font)
{
  const GtkCssToken *token;
  PangoFontDescription *desc;
  PangoFontMap *font_map;
  PangoContext *context;
  PangoFont *font;

  token = gtk_css_parser_get_token (parser);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
    return FALSE;

  desc = pango_font_description_from_string (token->string.string);
  font_map = pango_cairo_font_map_get_default ();
  context = pango_font_map_create_context (font_map);
  font = pango_font_map_load_font (font_map, context, desc);

  pango_font_description_free (desc);
  g_object_unref (context);

  *((PangoFont**)out_font) = font;

  /* Skip font name token */
  gtk_css_parser_consume_token (parser);

  return parse_semicolon (parser);
}

static gboolean
parse_glyphs (GtkCssParser *parser,
              gpointer      out_glyphs)
{
  GArray *glyphs;
  PangoGlyphString *glyph_string;
  int i;

  glyphs = g_array_new (FALSE, TRUE, sizeof (double[5]));

  for (;;)
    {
      double values[5];

      /* We have 5 numbers per glyph */
      for (i = 0; i < 5; i ++)
        {
          if (!gtk_css_parser_consume_number (parser, &values[i]))
            return FALSE;
        }

      g_array_append_val (glyphs, values);

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        gtk_css_parser_skip (parser);
      else
        break;
    }

  glyph_string = pango_glyph_string_new ();
  pango_glyph_string_set_size (glyph_string, glyphs->len);

  for (i = 0; i < glyphs->len; i ++)
    {
      PangoGlyphInfo g;
      double *v = (double *)(glyphs->data + (i * sizeof (double) * 5));

      g.glyph = (guint)v[0];
      g.geometry.width = (int)v[1];
      g.geometry.x_offset = (int)v[2];
      g.geometry.y_offset = (int)v[3];
      g.attr.is_cluster_start = (int)v[4];

      glyph_string->glyphs[i] = g;
    }

  g_array_free (glyphs, TRUE);

  *((PangoGlyphString **)out_glyphs) = glyph_string;

  return parse_semicolon (parser);
}

static gboolean
parse_node (GtkCssParser *parser, gpointer out_node);

static GskRenderNode *
parse_container_node (GtkCssParser *parser)
{
  GskRenderNode *node;
  GPtrArray *nodes;
  const GtkCssToken *token;

  nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      node = NULL;
      if (parse_node (parser, &node))
        {
          g_ptr_array_add (nodes, node);
        }
      else
        {
          gtk_css_parser_skip_until (parser, GTK_CSS_TOKEN_OPEN_CURLY);
          gtk_css_parser_skip (parser);
        }
    }

  node = gsk_container_node_new ((GskRenderNode **) nodes->pdata, nodes->len);

  g_ptr_array_unref (nodes);

  return node;
}

static void
parse_declarations_sync (GtkCssParser *parser)
{
  const GtkCssToken *token;

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_SEMICOLON) ||
          gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
        {
          gtk_css_parser_skip (parser);
          break;
        }
      gtk_css_parser_skip (parser);
    }
}

static guint
parse_declarations (GtkCssParser      *parser,
                    const Declaration *declarations,
                    guint              n_declarations)
{
  guint parsed = 0;
  guint i;
  const GtkCssToken *token;

  g_assert (n_declarations < 8 * sizeof (guint));

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      for (i = 0; i < n_declarations; i++)
        {
          if (gtk_css_token_is_ident (token, declarations[i].name))
            {
              gtk_css_parser_consume_token (parser);
              token = gtk_css_parser_get_token (parser);
              if (!gtk_css_token_is (token, GTK_CSS_TOKEN_COLON))
                {
                  gtk_css_parser_error_syntax (parser, "Expected ':' after variable declaration");
                  parse_declarations_sync (parser);
                }
              else
                {
                  gtk_css_parser_consume_token (parser);
                  if (parsed & (1 << i))
                    gtk_css_parser_warn_syntax (parser, "Variable \"%s\" defined multiple times", declarations[i].name);
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
          if (gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
            gtk_css_parser_error_syntax (parser, "No variable named \"%s\"", token->string.string);
          else
            gtk_css_parser_error_syntax (parser, "Expected a variable name");
          parse_declarations_sync (parser);
        }
    }

  return parsed;
}

static GskRenderNode *
parse_color_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  GdkRGBA color = { 0, 0, 0, 1 };
  const Declaration declarations[] = {
    { "bounds", parse_rect, &bounds },
    { "color", parse_color, &color },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  return gsk_color_node_new (&color, &bounds);
}

static GskRenderNode *
parse_linear_gradient_node (GtkCssParser *parser)
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
      gtk_css_parser_error_syntax (parser, "No color stops given");
      return NULL;
    }

  result = gsk_linear_gradient_node_new (&bounds, &start, &end, (GskColorStop *) stops->data, stops->len);

  g_array_free (stops, TRUE);

  return result;
}

static GskRenderNode *
parse_inset_shadow_node (GtkCssParser *parser)
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
parse_border_node (GtkCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 0, 0);
  graphene_rect_t widths = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  GdkRGBA colors[4] = { { 0, 0, 0, 0 }, {0, 0, 0, 0}, {0, 0, 0, 0}, { 0, 0, 0, 0 } };
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, &outline },
    { "widths", parse_rect,  &widths },
    { "colors", parse_colors4, &colors }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  return gsk_border_node_new (&outline, (float*)&widths, colors);
}

static GskRenderNode *
parse_texture_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  struct {
    guchar *data;
    gsize data_len;
  } texture_data = { NULL, 0 };
  double width = 0.0;
  double height = 0.0;
  const Declaration declarations[] = {
    { "bounds", parse_rect, &bounds },
    { "width", parse_double, &width },
    { "height", parse_double, &height },
    { "texture", parse_data, &texture_data }
  };
  GdkTexture *texture;
  GdkPixbuf *pixbuf;
  GskRenderNode *node;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  pixbuf = gdk_pixbuf_new_from_data (texture_data.data,
                                     GDK_COLORSPACE_RGB,
                                     TRUE,
                                     8,
                                     (int)width,
                                     (int)height,
                                     4 * (int)width,
                                     (GdkPixbufDestroyNotify)g_free, NULL);

  texture = gdk_texture_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);
  node = gsk_texture_node_new (texture, &bounds);
  g_object_unref (texture);

  return node;
}

static GskRenderNode *
parse_outset_shadow_node (GtkCssParser *parser)
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
parse_transform_node (GtkCssParser *parser)
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
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
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
parse_opacity_node (GtkCssParser *parser)
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
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_opacity_node_new (child, opacity);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_color_matrix_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  graphene_matrix_t matrix;
  GskTransform *transform = NULL;
  graphene_rect_t offset_rect = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  graphene_vec4_t offset;
  const Declaration declarations[] = {
    { "matrix", parse_transform, &transform },
    { "offset", parse_rect, &offset_rect },
    { "child", parse_node, &child }
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  graphene_vec4_init (&offset,
                      offset_rect.origin.x, offset_rect.origin.y,
                      offset_rect.size.width, offset_rect.size.height);

  gsk_transform_to_matrix (transform, &matrix);

  result = gsk_color_matrix_node_new (child, &matrix, &offset);

  gsk_transform_unref (transform);
  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_cross_fade_node (GtkCssParser *parser)
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
        gtk_css_parser_error_syntax (parser, "Missing \"start\" property definition");
      if (end == NULL)
        gtk_css_parser_error_syntax (parser, "Missing \"end\" property definition");
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
parse_blend_node (GtkCssParser *parser)
{
  GskRenderNode *bottom = NULL;
  GskRenderNode *top = NULL;
  GskBlendMode mode = GSK_BLEND_MODE_DEFAULT;
  const Declaration declarations[] = {
    { "mode", parse_blend_mode, &mode },
    { "bottom", parse_node, &bottom },
    { "top", parse_node, &top },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (bottom == NULL || top == NULL)
    {
      if (bottom == NULL)
        gtk_css_parser_error_syntax (parser, "Missing \"bottom\" property definition");
      if (top == NULL)
        gtk_css_parser_error_syntax (parser, "Missing \"top\" property definition");
      g_clear_pointer (&bottom, gsk_render_node_unref);
      g_clear_pointer (&top, gsk_render_node_unref);
      return NULL;
    }

  result = gsk_blend_node_new (bottom, top, mode);

  gsk_render_node_unref (bottom);
  gsk_render_node_unref (top);

  return result;
}

static GskRenderNode *
parse_text_node (GtkCssParser *parser)
{
  PangoFont *font = NULL;
  double x = 0;
  double y = 0;
  GdkRGBA color = { 0, 0, 0, 0 };
  PangoGlyphString *glyphs = NULL;
  const Declaration declarations[] = {
    { "font", parse_font, &font },
    { "x", parse_double, &x },
    { "y", parse_double, &y },
    { "color", parse_color, &color },
    { "glyphs", parse_glyphs, &glyphs }
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));

  if (!font || !glyphs)
    return NULL;

  result = gsk_text_node_new (font, glyphs, &color, x, y);

  g_object_unref (font);
  pango_glyph_string_free (glyphs);

  return result;
}

static GskRenderNode *
parse_blur_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  double blur_radius = 0.0;
  const Declaration declarations[] = {
    { "blur", parse_double, &blur_radius },
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_blur_node_new (child, blur_radius);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_clip_node (GtkCssParser *parser)
{
  graphene_rect_t clip = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rect, &clip },
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_clip_node_new (child, &clip);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_rounded_clip_node (GtkCssParser *parser)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 0, 0);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, &clip },
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_rounded_clip_node_new (child, &clip);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_shadow_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  GArray *shadows = g_array_new (FALSE, TRUE, sizeof (GskShadow));
  const Declaration declarations[] = {
    { "child", parse_node, &child },
    { "shadows", parse_shadows, shadows }
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  if (shadows->len == 0)
    {
      gtk_css_parser_error_syntax (parser, "Need at least one shadow");
      return child;
    }

  result = gsk_shadow_node_new (child, (GskShadow *)shadows->data, shadows->len);

  g_array_free (shadows, TRUE);
  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_debug_node (GtkCssParser *parser)
{
  char *message = NULL;
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "message", parse_string, &message},
    { "child", parse_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS(declarations));
  if (child == NULL)
    {
      gtk_css_parser_error_syntax (parser, "Missing \"child\" property definition");
      return NULL;
    }

  result = gsk_debug_node_new (child, message);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
parse_node (GtkCssParser *parser,
            gpointer      out_node)
{
  static struct {
    const char *name;
    GskRenderNode * (* func) (GtkCssParser *);
  } node_parsers[] = {
    { "container", parse_container_node },
    { "color", parse_color_node },
    { "linear-gradient", parse_linear_gradient_node },
    { "border", parse_border_node },
    { "texture", parse_texture_node },
    { "inset-shadow", parse_inset_shadow_node },
    { "outset-shadow", parse_outset_shadow_node },
    { "transform", parse_transform_node },
    { "opacity", parse_opacity_node },
    { "color-matrix", parse_color_matrix_node },
    { "clip", parse_clip_node },
    { "rounded-clip", parse_rounded_clip_node },
    { "shadow", parse_shadow_node },
    { "cross-fade", parse_cross_fade_node },
    { "text", parse_text_node },
    { "blur", parse_blur_node },
    { "debug", parse_debug_node },
    { "blend", parse_blend_node },
#if 0
    { "repeat", parse_repeat_node },
    { "cairo", parse_cairo_node },
#endif

  };
  GskRenderNode **node_p = out_node;
  const GtkCssToken *token;
  guint i;

  token = gtk_css_parser_get_token (parser);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_IDENT))
    {
      gtk_css_parser_error_syntax (parser, "Expected a node name");
      return FALSE;
    }

  for (i = 0; i < G_N_ELEMENTS (node_parsers); i++)
    {
      if (gtk_css_token_is_ident (token, node_parsers[i].name))
        {
          GskRenderNode *node;

          gtk_css_parser_consume_token (parser);
          token = gtk_css_parser_get_token (parser);
          if (!gtk_css_token_is (token, GTK_CSS_TOKEN_OPEN_CURLY))
            {
              gtk_css_parser_error_syntax (parser, "Expected '{' after node name");
              return FALSE;
            }
          gtk_css_parser_start_block (parser);
          node = node_parsers[i].func (parser);
          if (node)
            {
              token = gtk_css_parser_get_token (parser);
              if (!gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
                gtk_css_parser_error_syntax (parser, "Expected '}' at end of node definition");
              g_clear_pointer (node_p, gsk_render_node_unref);
              *node_p = node;
            }
          gtk_css_parser_end_block (parser);

          return node != NULL;
        }
    }

  gtk_css_parser_error_value (parser, "\"%s\" is not a valid node name", token->string.string);
  return FALSE;
}

static void
gsk_render_node_parser_error (GtkCssParser         *parser,
                              const GtkCssLocation *start,
                              const GtkCssLocation *end,
                              const GError         *error,
                              gpointer              user_data)
{
  struct {
    GskParseErrorFunc error_func;
    gpointer user_data;
  } *error_func_pair = user_data;

  if (error_func_pair->error_func)
    {
      GtkCssSection *section = gtk_css_section_new (gtk_css_parser_get_file (parser), start, end);

      error_func_pair->error_func (section, error, error_func_pair->user_data);
      gtk_css_section_unref (section);
    }
}

GskRenderNode *
gsk_render_node_deserialize_from_bytes (GBytes            *bytes,
                                        GskParseErrorFunc  error_func,
                                        gpointer           user_data)
{
  GskRenderNode *root = NULL;
  GtkCssParser *parser;
  struct {
    GskParseErrorFunc error_func;
    gpointer user_data;
  } error_func_pair = { error_func, user_data };

  parser = gtk_css_parser_new_for_bytes (bytes, NULL, NULL, gsk_render_node_parser_error,
                                         &error_func_pair, NULL);
  root = parse_container_node (parser);

  if (root && gsk_container_node_get_n_children (root) == 1)
    {
      GskRenderNode *child = gsk_container_node_get_child (root, 0);

      gsk_render_node_ref (child);
      gsk_render_node_unref (root);
      root = child;
    }

  gtk_css_parser_unref (parser);

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
#undef IDENT_LEVEL

static void
start_node (Printer    *self,
            const char *node_name)
{
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
string_append_double (GString *string,
                      double   d)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  g_ascii_formatd (buf, G_ASCII_DTOSTR_BUF_SIZE, "%g", d);
  g_string_append (string, buf);
}


static void
append_rect (GString               *str,
             const graphene_rect_t *r)
{
  string_append_double (str, r->origin.x);
  g_string_append_c (str, ' ');
  string_append_double (str, r->origin.y);
  g_string_append_c (str, ' ');
  string_append_double (str, r->size.width);
  g_string_append_c (str, ' ');
  string_append_double (str, r->size.height);
}

static void
append_rounded_rect (GString              *str,
                     const GskRoundedRect *r)
{
  append_rect (str, &r->bounds);

  if (!gsk_rounded_rect_is_rectilinear (r))
    {
      gboolean all_the_same = TRUE;
      gboolean all_square = TRUE;
      float w = r->corner[0].width;
      float h = r->corner[0].height;
      int i;

      for (i = 1; i < 4; i ++)
        {
          if (r->corner[i].width != w ||
              r->corner[i].height != h)
            all_the_same = FALSE;

          if (r->corner[i].width != r->corner[i].height)
            all_square = FALSE;

        }

      g_string_append (str, " / ");

      if (all_the_same)
        {
          string_append_double (str, w);
        }
      else if (all_square)
        {
          string_append_double (str, r->corner[0].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[1].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[2].width);
          g_string_append_c (str, ' ');
          string_append_double (str, r->corner[3].width);
        }
      else
        {
          for (i = 0; i < 4; i ++)
            {
              string_append_double (str, r->corner[i].width);
              g_string_append_c (str, ' ');
            }

          g_string_append (str, "/ ");

          for (i = 0; i < 3; i ++)
            {
              string_append_double (str, r->corner[i].height);
              g_string_append_c (str, ' ');
            }

          string_append_double (str, r->corner[3].height);
        }
    }
}

static void
append_rgba (GString       *str,
             const GdkRGBA *rgba)
{
  char *rgba_str = gdk_rgba_to_string (rgba);

  g_string_append (str, rgba_str);

  g_free (rgba_str);
}

static void
append_point (GString                *str,
              const graphene_point_t *p)
{
  string_append_double (str, p->x);
  g_string_append_c (str, ' ');
  string_append_double (str, p->y);
}

static void
append_vec4 (GString               *str,
             const graphene_vec4_t *v)
{
  string_append_double (str, graphene_vec4_get_x (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_y (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_z (v));
  g_string_append_c (str, ' ');
  string_append_double (str, graphene_vec4_get_w (v));
}

static void
append_float_param (Printer    *p,
                    const char *param_name,
                    float       value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  string_append_double (p->str, value);
  g_string_append (p->str, ";\n");
}

static void
append_rgba_param (Printer       *p,
                   const char    *param_name,
                   const GdkRGBA *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_rgba (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_rect_param (Printer               *p,
                   const char            *param_name,
                   const graphene_rect_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_rect (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_rounded_rect_param (Printer              *p,
                           const char           *param_name,
                           const GskRoundedRect *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_rounded_rect (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_point_param (Printer                *p,
                    const char             *param_name,
                    const graphene_point_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_point (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_vec4_param (Printer               *p,
                   const char            *param_name,
                   const graphene_vec4_t *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  append_vec4 (p->str, value);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void
append_matrix_param (Printer                 *p,
                     const char              *param_name,
                     const graphene_matrix_t *value)
{
  GskTransform *transform = NULL;

  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);

  transform = gsk_transform_matrix (transform, value);
  gsk_transform_print (transform,p->str);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');

  gsk_transform_unref (transform);
}

static void
append_transform_param (Printer      *p,
                        const char   *param_name,
                        GskTransform *transform)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  gsk_transform_print (transform, p->str);
  g_string_append_c (p->str, ';');
  g_string_append_c (p->str, '\n');
}

static void render_node_print (Printer       *p,
                               GskRenderNode *node);

static void
append_node_param (Printer       *p,
                   const char    *param_name,
                   GskRenderNode *node)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  render_node_print (p, node);
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

            /* Only in container nodes do we want nodes to be indented. */
            _indent (p);
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
        start_node (p, "cross-fade");

        append_float_param (p, "progress", gsk_cross_fade_node_get_progress (node));
        append_node_param (p, "start", gsk_cross_fade_node_get_start_child (node));
        append_node_param (p, "end", gsk_cross_fade_node_get_end_child (node));

        end_node (p);
      }
      break;

    case GSK_LINEAR_GRADIENT_NODE:
      {
        const guint n_stops = gsk_linear_gradient_node_get_n_color_stops (node);
        const GskColorStop *stop;
        int i;

        start_node (p, "linear-gradient");

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "start", gsk_linear_gradient_node_peek_start (node));
        append_point_param (p, "end", gsk_linear_gradient_node_peek_end (node));

        _indent (p);
        g_string_append (p->str, "stops:");
        for (i = 0; i < n_stops - 1; i ++)
          {
            stop = gsk_linear_gradient_node_peek_color_stops (node) + i;

            g_string_append_c (p->str, ' ');
            string_append_double (p->str, stop->offset);
            g_string_append_c (p->str, ' ');
            append_rgba (p->str, &stop->color);
            g_string_append_c (p->str, ',');
          }

        /* Last one, no comma */
        stop = gsk_linear_gradient_node_peek_color_stops (node) + n_stops - 1;
        string_append_double (p->str, stop->offset);
        g_string_append_c (p->str, ' ');
        append_rgba (p->str, &stop->color);
        g_string_append (p->str, ";\n");

        end_node (p);
      }
      break;

    case GSK_OPACITY_NODE:
      {
        start_node (p, "opacity");

        append_float_param (p, "opacity", gsk_opacity_node_get_opacity (node));
        append_node_param (p, "child", gsk_opacity_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        start_node (p, "outset-shadow");

        append_rounded_rect_param (p, "outline", gsk_outset_shadow_node_peek_outline (node));
        append_rgba_param (p, "color", gsk_outset_shadow_node_peek_color (node));
        append_float_param (p, "dx", gsk_outset_shadow_node_get_dx (node));
        append_float_param (p, "dy", gsk_outset_shadow_node_get_dy (node));
        append_float_param (p, "spread", gsk_outset_shadow_node_get_spread (node));
        append_float_param (p, "blur", gsk_outset_shadow_node_get_blur_radius (node));

        end_node (p);
      }
      break;

    case GSK_CLIP_NODE:
      {
        start_node (p, "clip");

        append_rect_param (p, "clip", gsk_clip_node_peek_clip (node));
        append_node_param (p, "child", gsk_clip_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        start_node (p, "rounded-clip");

        append_rounded_rect_param (p, "clip", gsk_rounded_clip_node_peek_clip (node));
        append_node_param (p, "child", gsk_rounded_clip_node_get_child (node));


        end_node (p);
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        start_node (p, "transform");

        append_transform_param (p, "transform", gsk_transform_node_get_transform (node));
        append_node_param (p, "child", gsk_transform_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_COLOR_MATRIX_NODE:
      {
        start_node (p, "color-matrix");

        append_matrix_param (p, "matrix", gsk_color_matrix_node_peek_color_matrix (node));
        append_vec4_param (p, "offset", gsk_color_matrix_node_peek_color_offset (node));
        append_node_param (p, "child", gsk_color_matrix_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BORDER_NODE:
      {
        start_node (p, "border");

        append_rounded_rect_param (p, "outline", gsk_border_node_peek_outline (node));

        _indent (p);
        g_string_append (p->str, "widths: ");
        string_append_double (p->str, gsk_border_node_peek_widths (node)[0]);
        g_string_append_c (p->str, ' ');
        string_append_double (p->str, gsk_border_node_peek_widths (node)[1]);
        g_string_append_c (p->str, ' ');
        string_append_double (p->str, gsk_border_node_peek_widths (node)[2]);
        g_string_append_c (p->str, ' ');
        string_append_double (p->str, gsk_border_node_peek_widths (node)[3]);
        g_string_append (p->str, ";\n");

        _indent (p);
        g_string_append (p->str, "colors: ");
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[0]);
        g_string_append_c (p->str, ' ');
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[1]);
        g_string_append_c (p->str, ' ');
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[2]);
        g_string_append_c (p->str, ' ');
        append_rgba (p->str, &gsk_border_node_peek_colors (node)[3]);
        g_string_append (p->str, ";\n");

        end_node (p);
      }
      break;

    case GSK_SHADOW_NODE:
      {
        const guint n_shadows = gsk_shadow_node_get_n_shadows (node);
        int i;

        start_node (p, "shadow");

        _indent (p);
        g_string_append (p->str, "shadows: ");
        for (i = 0; i < n_shadows; i ++)
          {
            const GskShadow *s = gsk_shadow_node_peek_shadow (node, i);
            char *color;

            color = gdk_rgba_to_string (&s->color);
            g_string_append (p->str, color);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->dx);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->dy);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->radius);

            if (i < n_shadows - 1)
              g_string_append (p->str, ", ");

            g_free (color);
          }

        g_string_append_c (p->str, ';');
        g_string_append_c (p->str, '\n');

        append_node_param (p, "child", gsk_shadow_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_INSET_SHADOW_NODE:
      {
        start_node (p, "inset-shadow");

        append_rounded_rect_param (p, "outline", gsk_inset_shadow_node_peek_outline (node));
        append_rgba_param (p, "color", gsk_inset_shadow_node_peek_color (node));
        append_float_param (p, "dx", gsk_inset_shadow_node_get_dx (node));
        append_float_param (p, "dy", gsk_inset_shadow_node_get_dy (node));
        append_float_param (p, "spread", gsk_inset_shadow_node_get_spread (node));
        append_float_param (p, "blur", gsk_inset_shadow_node_get_blur_radius (node));

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
        /* TODO: width and height here are unnecessary and can later be computed from the data length? */
        append_float_param (p, "width", gdk_texture_get_width (texture));
        append_float_param (p, "height", gdk_texture_get_height (texture));

        stride = 4 * gdk_texture_get_width (texture);
        len = sizeof (guchar) * stride * gdk_texture_get_height (texture);
        data = g_malloc (len);
        gdk_texture_download (texture, data, stride);

        b64 = g_base64_encode (data, len);

        _indent (p);
        g_string_append_printf (p->str, "texture: \"data:;base64,%s\";\n", b64);
        end_node (p);

        g_free (b64);
        g_free (data);
      }
      break;

    case GSK_TEXT_NODE:
      {
        const guint n_glyphs = gsk_text_node_get_num_glyphs (node);
        const PangoGlyphInfo *glyph;
        PangoFontDescription *desc;
        char *font_name;
        guint i;
        start_node (p, "text");

        _indent (p);
        desc = pango_font_describe ((PangoFont *)gsk_text_node_peek_font (node));;
        font_name = pango_font_description_to_string (desc);
        g_string_append_printf (p->str, "font: \"%s\";\n", font_name);
        g_free (font_name);
        pango_font_description_free (desc);

        append_float_param (p, "x", gsk_text_node_get_x (node));
        append_float_param (p, "y", gsk_text_node_get_y (node));
        append_rgba_param (p, "color", gsk_text_node_peek_color (node));

        _indent (p);
        g_string_append (p->str, "glyphs: ");
        glyph = gsk_text_node_peek_glyphs (node);
        g_string_append_printf (p->str, "%u %i %i %i %i",
                                glyph->glyph, glyph->geometry.width,
                                glyph->geometry.x_offset, glyph->geometry.y_offset,
                                glyph->attr.is_cluster_start);

        for (i = 1; i < n_glyphs; i ++)
          {
            glyph = gsk_text_node_peek_glyphs (node) + i;
            g_string_append_printf (p->str, ", %u %i %i %i %i",
                                    glyph->glyph, glyph->geometry.width,
                                    glyph->geometry.x_offset, glyph->geometry.y_offset,
                                    glyph->attr.is_cluster_start);
          }

        g_string_append_c (p->str, ';');
        g_string_append_c (p->str, '\n');

        end_node (p);
      }
      break;

    case GSK_DEBUG_NODE:
      {
        start_node (p, "debug");

        _indent (p);
        /* TODO: We potentially need to escape certain characters in the message */
        g_string_append_printf (p->str, "message: \"%s\";\n", gsk_debug_node_get_message (node));

        append_node_param (p, "child", gsk_debug_node_get_child (node));
        end_node (p);
      }
      break;

    case GSK_BLUR_NODE:
      {
        start_node (p, "blur");

        append_float_param (p, "blur", gsk_blur_node_get_radius (node));
        append_node_param (p, "child", gsk_blur_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_REPEAT_NODE:
      {
        start_node (p, "repeat");
        append_rect_param (p, "bounds", &node->bounds);
        append_rect_param (p, "child-bounds", gsk_repeat_node_peek_child_bounds (node));

        append_node_param (p, "child", gsk_repeat_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BLEND_NODE:
      {
        GskBlendMode mode = gsk_blend_node_get_blend_mode (node);
        guint i;

        start_node (p, "blend");

        _indent (p);
        for (i = 0; i < G_N_ELEMENTS (blend_modes); i++)
          {
            if (blend_modes[i].mode == mode)
              {
                g_string_append_printf (p->str, "mode: %s;\n", blend_modes[i].name);
                break;
              }
          }
        append_node_param (p, "top", gsk_blend_node_get_top_child (node));
        append_node_param (p, "bottom", gsk_blend_node_get_bottom_child (node));

        end_node (p);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    case GSK_CAIRO_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    default:
      g_error ("Unhandled node: %s", node->node_class->type_name);
      break;
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
