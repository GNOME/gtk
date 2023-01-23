/*
 * Copyright © 2019 Benjamin Otte
 *                  Timm Bäder
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 *          Timm Bäder <mail@baedert.org>
 */

#include "config.h"

#include "gskrendernodeparserprivate.h"

#include "gskroundedrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gsktransformprivate.h"

#include "gdk/gdkrgbaprivate.h"
#include "gdk/gdktextureprivate.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcssdataurlprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/css/gtkcssserializerprivate.h"

#ifdef CAIRO_HAS_SCRIPT_SURFACE
#include <cairo-script.h>
#endif
#ifdef HAVE_CAIRO_SCRIPT_INTERPRETER
#include <cairo-script-interpreter.h>
#endif

typedef struct _Declaration Declaration;

struct _Declaration
{
  const char *name;
  gboolean (* parse_func) (GtkCssParser *parser, gpointer result);
  void (* clear_func) (gpointer data);
  gpointer result;
};

static gboolean
parse_rect (GtkCssParser    *parser,
            gpointer         out_rect)
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
parse_vec4 (GtkCssParser    *parser,
            gpointer         out_vec4)
{
  double numbers[4];

  if (!gtk_css_parser_consume_number (parser, &numbers[0]) ||
      !gtk_css_parser_consume_number (parser, &numbers[1]) ||
      !gtk_css_parser_consume_number (parser, &numbers[2]) ||
      !gtk_css_parser_consume_number (parser, &numbers[3]))
    return FALSE;

  graphene_vec4_init (out_vec4, numbers[0], numbers[1], numbers[2], numbers[3]);

  return TRUE;
}

static gboolean
parse_texture (GtkCssParser *parser,
               gpointer      out_data)
{
  GdkTexture *texture;
  GError *error = NULL;
  GtkCssLocation start_location;
  char *url, *scheme;

  start_location = *gtk_css_parser_get_start_location (parser);
  url = gtk_css_parser_consume_url (parser);
  if (url == NULL)
    return FALSE;

  scheme = g_uri_parse_scheme (url);
  if (scheme && g_ascii_strcasecmp (scheme, "data") == 0)
    {
      GBytes *bytes;

      bytes = gtk_css_data_url_parse (url, NULL, &error);
      if (bytes)
        {
          texture = gdk_texture_new_from_bytes (bytes, &error);
          g_bytes_unref (bytes);
        }
      else
        {
          texture = NULL;
        }
    }
  else
    {
      GFile *file;

      file = gtk_css_parser_resolve_url (parser, url);

      if (file)
        {
          texture = gdk_texture_new_from_file (file, &error);
          g_object_unref (file);
        }
      else
        {
          texture = NULL;
        }
    }

  g_free (scheme);
  g_free (url);

  if (texture == NULL)
    {
      if (error)
        {
          gtk_css_parser_emit_error (parser,
                                     &start_location,
                                     gtk_css_parser_get_end_location (parser),
                                     error);
          g_clear_error (&error);
        }
      return FALSE;
    }

  *(GdkTexture **) out_data = texture;
  return TRUE;
}

static void
clear_texture (gpointer inout_texture)
{
  g_clear_object ((GdkTexture **) inout_texture);
}

static cairo_surface_t *
csi_hooks_surface_create (void            *closure,
                          cairo_content_t  content,
                          double           width,
                          double           height,
                          long             uid)
{
  return cairo_surface_create_similar (closure, content, width, height);
}

static const cairo_user_data_key_t csi_hooks_key;

static cairo_t *
csi_hooks_context_create (void            *closure,
                          cairo_surface_t *surface)
{
  cairo_t *cr = cairo_create (surface);

  cairo_set_user_data (cr,
                       &csi_hooks_key,
                       cairo_surface_reference (surface),
                       (cairo_destroy_func_t) cairo_surface_destroy);

  return cr;
}

static void
csi_hooks_context_destroy (void *closure,
                           void *ptr)
{
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_get_user_data (ptr, &csi_hooks_key);
  cr = cairo_create (closure);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);
}

static gboolean
parse_script (GtkCssParser *parser,
              gpointer      out_data)
{
#ifdef HAVE_CAIRO_SCRIPT_INTERPRETER
  GError *error = NULL;
  GBytes *bytes;
  GtkCssLocation start_location;
  char *url, *scheme;
  cairo_script_interpreter_t *csi;
  cairo_script_interpreter_hooks_t hooks = {
    .surface_create = csi_hooks_surface_create,
    .context_create = csi_hooks_context_create,
    .context_destroy = csi_hooks_context_destroy,
  };

  start_location = *gtk_css_parser_get_start_location (parser);
  url = gtk_css_parser_consume_url (parser);
  if (url == NULL)
    return FALSE;

  scheme = g_uri_parse_scheme (url);
  if (scheme && g_ascii_strcasecmp (scheme, "data") == 0)
    {
      bytes = gtk_css_data_url_parse (url, NULL, &error);
    }
  else
    {
      GFile *file;

      file = gtk_css_parser_resolve_url (parser, url);
      bytes = g_file_load_bytes (file, NULL, NULL, &error);
      g_object_unref (file);
    }

  g_free (scheme);
  g_free (url);

  if (bytes == NULL)
    {
      gtk_css_parser_emit_error (parser,
                                 &start_location,
                                 gtk_css_parser_get_end_location (parser),
                                 error);
      g_clear_error (&error);
      return FALSE;
    }

  hooks.closure = cairo_recording_surface_create (CAIRO_CONTENT_COLOR_ALPHA, NULL);
  csi = cairo_script_interpreter_create ();
  cairo_script_interpreter_install_hooks (csi, &hooks);
  cairo_script_interpreter_feed_string (csi, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
  g_bytes_unref (bytes);
  if (cairo_surface_status (hooks.closure) != CAIRO_STATUS_SUCCESS)
    {
      gtk_css_parser_error_value (parser, "Invalid Cairo script: %s", cairo_status_to_string (cairo_surface_status (hooks.closure)));
      cairo_script_interpreter_destroy (csi);
      return FALSE;
    }
  if (cairo_script_interpreter_destroy (csi) != CAIRO_STATUS_SUCCESS)
    {
      gtk_css_parser_error_value (parser, "Invalid Cairo script");
      cairo_surface_destroy (hooks.closure);
      return FALSE;
    }

  *(cairo_surface_t **) out_data = hooks.closure;
  return TRUE;
#else
  gtk_css_parser_warn (parser,
                       GTK_CSS_PARSER_WARNING_UNIMPLEMENTED,
                       gtk_css_parser_get_block_location (parser),
                       gtk_css_parser_get_start_location (parser),
                       "GTK was compiled with script interpreter support. Using fallback pixel data for Cairo node.");
  *(cairo_surface_t **) out_data = NULL;
  return TRUE;
#endif
}

static void
clear_surface (gpointer inout_surface)
{
  g_clear_pointer ((cairo_surface_t **) inout_surface, cairo_surface_destroy);
}

static gboolean
parse_rounded_rect (GtkCssParser *parser,
                    gpointer      out_rect)
{
  graphene_rect_t r;
  graphene_size_t corners[4];
  double d;
  guint i;

  if (!parse_rect (parser, &r))
    return FALSE;

  if (!gtk_css_parser_try_delim (parser, '/'))
    {
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

  gsk_rounded_rect_init (out_rect, &r, &corners[0], &corners[1], &corners[2], &corners[3]);

  return TRUE;
}

static gboolean
parse_color (GtkCssParser *parser,
             gpointer      out_color)
{
  return gdk_rgba_parser_parse (parser, out_color);
}

static gboolean
parse_double (GtkCssParser *parser,
              gpointer      out_double)
{
  return gtk_css_parser_consume_number (parser, out_double);
}

static gboolean
parse_point (GtkCssParser *parser,
             gpointer      out_point)
{
  double x, y;

  if (!gtk_css_parser_consume_number (parser, &x) ||
      !gtk_css_parser_consume_number (parser, &y))
    return FALSE;

  graphene_point_init (out_point, x, y);

  return TRUE;
}

static gboolean
parse_transform (GtkCssParser *parser,
                 gpointer      out_transform)
{
  GskTransform *transform;

  if (!gsk_transform_parser_parse (parser, &transform))
    {
      gsk_transform_unref (transform);
      return FALSE;
    }

  *(GskTransform **) out_transform = transform;

  return TRUE;
}

static void
clear_transform (gpointer inout_transform)
{
  g_clear_pointer ((GskTransform **) inout_transform, gsk_transform_unref);
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

  s = g_strdup (gtk_css_token_get_string (token));
  gtk_css_parser_consume_token (parser);

  g_free (*(char **) out_string);
  *(char **) out_string = s;

  return TRUE;
}

static void
clear_string (gpointer inout_string)
{
  g_clear_pointer ((char **) inout_string, g_free);
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
     double dval;

      if (!gtk_css_parser_consume_number (parser, &dval))
        goto error;

      stop.offset = dval;

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

  return TRUE;

error:
  g_array_free (stops, TRUE);
  return FALSE;
}

static void
clear_stops (gpointer inout_stops)
{
  GArray **stops = (GArray **) inout_stops;

  if (*stops)
    {
      g_array_free (*stops, TRUE);
      *stops = NULL;
    }
}

static gboolean
parse_float4 (GtkCssParser *parser,
              gpointer      out_floats)
{
  float *floats = (float *) out_floats;
  double d[4];
  int i;

  for (i = 0; i < 4 && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF); i ++)
    {
      if (!gtk_css_parser_consume_number (parser, &d[i]))
        return FALSE;
    }
  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a color");
      return FALSE;
    }
  for (; i < 4; i++)
    {
      d[i] = d[(i - 1) >> 1];
    }

  for (i = 0; i < 4; i++)
    {
      floats[i] = d[i];
    }

  return TRUE;
}

static gboolean
parse_colors4 (GtkCssParser *parser,
               gpointer      out_colors)
{
  GdkRGBA colors[4];
  int i;

  for (i = 0; i < 4 && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF); i ++)
    {
      if (!gdk_rgba_parser_parse (parser, &colors[i]))
        return FALSE;
    }
  if (i == 0)
    {
      gtk_css_parser_error_syntax (parser, "Expected a color");
      return FALSE;
    }
  for (; i < 4; i++)
    {
      colors[i] = colors[(i - 1) >> 1];
    }

  memcpy (out_colors, colors, sizeof (GdkRGBA) * 4);

  return TRUE;
}

static gboolean
parse_shadows (GtkCssParser *parser,
               gpointer      out_shadows)
{
  GArray *shadows = out_shadows;

  do
    {
      GskShadow shadow = { GDK_RGBA("000000"), 0, 0, 0 };
      double dx = 0, dy = 0, radius = 0;

      if (!gdk_rgba_parser_parse (parser, &shadow.color))
        gtk_css_parser_error_value (parser, "Expected shadow color");

      if (!gtk_css_parser_consume_number (parser, &dx))
        gtk_css_parser_error_value (parser, "Expected shadow x offset");

      if (!gtk_css_parser_consume_number (parser, &dy))
        gtk_css_parser_error_value (parser, "Expected shadow y offset");

      if (gtk_css_parser_has_number (parser))
        {
          if (!gtk_css_parser_consume_number (parser, &radius))
            gtk_css_parser_error_value (parser, "Expected shadow blur radius");
        }

      shadow.dx = dx;
      shadow.dy = dy;
      shadow.radius = radius;

      g_array_append_val (shadows, shadow);
    }
  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return TRUE;
}

static void
clear_shadows (gpointer inout_shadows)
{
  g_array_set_size (*(GArray **) inout_shadows, 0);
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
          *(GskBlendMode *) out_mode = blend_modes[i].mode;
          return TRUE;
        }
    }

  return FALSE;
}

static PangoFont *
font_from_string (const char *string)
{
  PangoFontDescription *desc;
  PangoFontMap *font_map;
  PangoContext *context;
  PangoFont *font;

  desc = pango_font_description_from_string (string);
  font_map = pango_cairo_font_map_get_default ();
  context = pango_font_map_create_context (font_map);
  font = pango_font_map_load_font (font_map, context, desc);

  pango_font_description_free (desc);
  g_object_unref (context);

  return font;
}

#define MIN_ASCII_GLYPH 32
#define MAX_ASCII_GLYPH 127 /* exclusive */
#define N_ASCII_GLYPHS (MAX_ASCII_GLYPH - MIN_ASCII_GLYPH)

static PangoGlyphString *
create_ascii_glyphs (PangoFont *font)
{
  PangoLanguage *language = pango_language_from_string ("en_US"); /* just pick one */
  PangoCoverage *coverage;
  PangoAnalysis not_a_hack = {
    .shape_engine = NULL, /* unused */
    .lang_engine = NULL, /* unused by pango_shape() */
    .font = font,
    .level = 0,
    .gravity = PANGO_GRAVITY_SOUTH,
    .flags = 0,
    .script = PANGO_SCRIPT_COMMON,
    .language = language,
    .extra_attrs = NULL
  };
  PangoGlyphString *result, *glyph_string;
  guint i;

  coverage = pango_font_get_coverage (font, language);
  for (i = MIN_ASCII_GLYPH; i < MAX_ASCII_GLYPH; i++)
    {
      if (!pango_coverage_get (coverage, i))
        break;
    }
  pango_coverage_unref (coverage);
  if (i < MAX_ASCII_GLYPH)
    return NULL;

  result = pango_glyph_string_new ();
  pango_glyph_string_set_size (result, N_ASCII_GLYPHS);
  glyph_string = pango_glyph_string_new ();
  for (i = MIN_ASCII_GLYPH; i < MAX_ASCII_GLYPH; i++)
    {
      const char text[2] = { i, 0 };
      PangoShapeFlags flags = 0;

      if (cairo_version () < CAIRO_VERSION_ENCODE (1, 17, 4))
        flags = PANGO_SHAPE_ROUND_POSITIONS;

      pango_shape_with_flags (text, 1,
                              text, 1,
                              &not_a_hack,
                              glyph_string,
                              flags);

      if (glyph_string->num_glyphs != 1)
        {
          pango_glyph_string_free (glyph_string);
          pango_glyph_string_free (result);
          return NULL;
        }
      result->glyphs[i - MIN_ASCII_GLYPH] = glyph_string->glyphs[0];
    }

  pango_glyph_string_free (glyph_string);

  return result;
}

static gboolean
parse_font (GtkCssParser *parser,
            gpointer      out_font)
{
  PangoFont *font;
  char *s;

  s = gtk_css_parser_consume_string (parser);
  if (s == NULL)
    return FALSE;

  font = font_from_string (s);
  if (font == NULL)
    {
      gtk_css_parser_error_syntax (parser, "This font does not exist.");
      return FALSE;
    }

  *((PangoFont**)out_font) = font;

  g_free (s);

  return TRUE;
}

static void
clear_font (gpointer inout_font)
{
  g_clear_object ((PangoFont **) inout_font);
}

static gboolean
parse_glyphs (GtkCssParser *parser,
              gpointer      out_glyphs)
{
  PangoGlyphString *glyph_string;

  glyph_string = pango_glyph_string_new ();

  do
    {
      PangoGlyphInfo gi = { 0, { 0, 0, 0}, { 1 } };
      double d, d2;
      int i;

      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
        {
          char *s = gtk_css_parser_consume_string (parser);

          for (i = 0; s[i] != 0; i++)
            {
              if (s[i] < MIN_ASCII_GLYPH || s[i] >= MAX_ASCII_GLYPH)
                {
                  gtk_css_parser_error_value (parser, "Unsupported character %d in string", i);
                }
              gi.glyph = PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH + s[i];
              pango_glyph_string_set_size (glyph_string, glyph_string->num_glyphs + 1);
              glyph_string->glyphs[glyph_string->num_glyphs - 1] = gi;
            }

          g_free (s);
        }
      else
        {
          if (!gtk_css_parser_consume_integer (parser, &i) ||
              !gtk_css_parser_consume_number (parser, &d))
            {
              pango_glyph_string_free (glyph_string);
              return FALSE;
            }
          gi.glyph = i;
          gi.geometry.width = (int) (d * PANGO_SCALE);

          if (gtk_css_parser_has_number (parser))
            {
              if (!gtk_css_parser_consume_number (parser, &d) ||
                  !gtk_css_parser_consume_number (parser, &d2))
                {
                  pango_glyph_string_free (glyph_string);
                  return FALSE;
                }
              gi.geometry.x_offset = (int) (d * PANGO_SCALE);
              gi.geometry.y_offset = (int) (d2 * PANGO_SCALE);

              if (gtk_css_parser_try_ident (parser, "same-cluster"))
                gi.attr.is_cluster_start = 0;
              else
                gi.attr.is_cluster_start = 1;

              if (gtk_css_parser_try_ident (parser, "color"))
                gi.attr.is_color = 1;
              else
                gi.attr.is_color = 0;
            }

          pango_glyph_string_set_size (glyph_string, glyph_string->num_glyphs + 1);
          glyph_string->glyphs[glyph_string->num_glyphs - 1] = gi;
        }
    }
  while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  *((PangoGlyphString **)out_glyphs) = glyph_string;

  return TRUE;
}

static void
clear_glyphs (gpointer inout_glyphs)
{
  g_clear_pointer ((PangoGlyphString **) inout_glyphs, pango_glyph_string_free);
}

static gboolean
parse_node (GtkCssParser *parser, gpointer out_node);

static void
clear_node (gpointer inout_node)
{
  g_clear_pointer ((GskRenderNode **) inout_node, gsk_render_node_unref);
}

static GskRenderNode *
parse_container_node (GtkCssParser *parser)
{
  GPtrArray *nodes;
  const GtkCssToken *token;
  GskRenderNode *node;

  nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);

  for (token = gtk_css_parser_get_token (parser);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (parser))
    {
      node = NULL;
      /* We don't want a semicolon here, but the parse_node function will figure
       * that out itself and return an error if we encounter one.
       */
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      if (parse_node (parser, &node))
        g_ptr_array_add (nodes, node);

      gtk_css_parser_end_block (parser);
    }

  node = gsk_container_node_new ((GskRenderNode **) nodes->pdata, nodes->len);

  g_ptr_array_unref (nodes);

  return node;
}

static guint
parse_declarations (GtkCssParser      *parser,
                    const Declaration *declarations,
                    guint              n_declarations)
{
  guint parsed = 0;
  guint i;

  g_assert (n_declarations < 8 * sizeof (guint));

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_start_semicolon_block (parser, GTK_CSS_TOKEN_OPEN_CURLY);

      for (i = 0; i < n_declarations; i++)
        {
          if (gtk_css_parser_try_ident (parser, declarations[i].name))
            {
              if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COLON))
                {
                  gtk_css_parser_error_syntax (parser, "Expected ':' after variable declaration");
                }
              else
                {
                  if (parsed & (1 << i))
                    {
                      gtk_css_parser_warn_syntax (parser, "Variable \"%s\" defined multiple times", declarations[i].name);
                      /* Unset, just to be sure */
                      parsed &= ~(1 << i);
                      if (declarations[i].clear_func)
                        declarations[i].clear_func (declarations[i].result);
                    }
                  if (!declarations[i].parse_func (parser, declarations[i].result))
                    {
                      /* nothing to do */
                    }
                  else if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
                    {
                      gtk_css_parser_error_syntax (parser, "Expected ';' at end of statement");
                      if (declarations[i].clear_func)
                        declarations[i].clear_func (declarations[i].result);
                    }
                  else
                    {
                      parsed |= (1 << i);
                    }
                }
              break;
            }
        }
      if (i == n_declarations)
        {
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
            gtk_css_parser_error_syntax (parser, "No variable named \"%s\"",
                                         gtk_css_token_get_string (gtk_css_parser_get_token (parser)));
          else
            gtk_css_parser_error_syntax (parser, "Expected a variable name");
        }

      gtk_css_parser_end_block (parser);
    }

  return parsed;
}

static GdkTexture *
create_default_texture (void)
{
  static const guint32 pixels[100] = {
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC,
    0, 0, 0, 0, 0, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC, 0xFFFF00CC };
  GBytes *bytes;
  GdkTexture *texture;

  bytes = g_bytes_new_static ((guchar *) pixels, 400);
  texture = gdk_memory_texture_new (10, 10, GDK_MEMORY_DEFAULT, bytes, 40);
  g_bytes_unref (bytes);

  return texture;
}

static GskRenderNode *
create_default_render_node (void)
{
  return gsk_color_node_new (&GDK_RGBA("FF00CC"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));
}

static GskRenderNode *
parse_color_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkRGBA color = GDK_RGBA("FF00CC");
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "color", parse_color, NULL, &color },
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  return gsk_color_node_new (&color, &bounds);
}

static GskRenderNode *
parse_linear_gradient_node_internal (GtkCssParser *parser,
                                     gboolean      repeating)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t start = GRAPHENE_POINT_INIT (0, 0);
  graphene_point_t end = GRAPHENE_POINT_INIT (0, 50);
  GArray *stops = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "start", parse_point, NULL, &start },
    { "end", parse_point, NULL, &end },
    { "stops", parse_stops, clear_stops, &stops },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskColorStop from = { 0.0, GDK_RGBA("AAFF00") };
      GskColorStop to = { 1.0, GDK_RGBA("FF00CC") };

      stops = g_array_new (FALSE, FALSE, sizeof (GskColorStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  if (repeating)
    result = gsk_repeating_linear_gradient_node_new (&bounds, &start, &end, (GskColorStop *) stops->data, stops->len);
  else
    result = gsk_linear_gradient_node_new (&bounds, &start, &end, (GskColorStop *) stops->data, stops->len);

  g_array_free (stops, TRUE);

  return result;
}

static GskRenderNode *
parse_linear_gradient_node (GtkCssParser *parser)
{
  return parse_linear_gradient_node_internal (parser, FALSE);
}

static GskRenderNode *
parse_repeating_linear_gradient_node (GtkCssParser *parser)
{
  return parse_linear_gradient_node_internal (parser, TRUE);
}

static GskRenderNode *
parse_radial_gradient_node_internal (GtkCssParser *parser,
                                     gboolean      repeating)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t center = GRAPHENE_POINT_INIT (25, 25);
  double hradius = 25.0;
  double vradius = 25.0;
  double start = 0;
  double end = 1.0;
  GArray *stops = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "center", parse_point, NULL, &center },
    { "hradius", parse_double, NULL, &hradius },
    { "vradius", parse_double, NULL, &vradius },
    { "start", parse_double, NULL, &start },
    { "end", parse_double, NULL, &end },
    { "stops", parse_stops, clear_stops, &stops },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskColorStop from = { 0.0, GDK_RGBA("AAFF00") };
      GskColorStop to = { 1.0, GDK_RGBA("FF00CC") };

      stops = g_array_new (FALSE, FALSE, sizeof (GskColorStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  if (repeating)
    result = gsk_repeating_radial_gradient_node_new (&bounds, &center, hradius, vradius, start, end,
                                                     (GskColorStop *) stops->data, stops->len);
  else
    result = gsk_radial_gradient_node_new (&bounds, &center, hradius, vradius, start, end,
                                           (GskColorStop *) stops->data, stops->len);

  g_array_free (stops, TRUE);

  return result;
}

static GskRenderNode *
parse_radial_gradient_node (GtkCssParser *parser)
{
  return parse_radial_gradient_node_internal (parser, FALSE);
}

static GskRenderNode *
parse_repeating_radial_gradient_node (GtkCssParser *parser)
{
  return parse_radial_gradient_node_internal (parser, TRUE);
}

static GskRenderNode *
parse_conic_gradient_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  graphene_point_t center = GRAPHENE_POINT_INIT (25, 25);
  double rotation = 0.0;
  GArray *stops = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "center", parse_point, NULL, &center },
    { "rotation", parse_double, NULL, &rotation },
    { "stops", parse_stops, clear_stops, &stops },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (stops == NULL)
    {
      GskColorStop from = { 0.0, GDK_RGBA("AAFF00") };
      GskColorStop to = { 1.0, GDK_RGBA("FF00CC") };

      stops = g_array_new (FALSE, FALSE, sizeof (GskColorStop));
      g_array_append_val (stops, from);
      g_array_append_val (stops, to);
    }

  result = gsk_conic_gradient_node_new (&bounds, &center, rotation,
                                        (GskColorStop *) stops->data, stops->len);

  g_array_free (stops, TRUE);

  return result;
}

static GskRenderNode *
parse_inset_shadow_node (GtkCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GdkRGBA color = GDK_RGBA("000000");
  double dx = 1, dy = 1, blur = 0, spread = 0;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "color", parse_color, NULL, &color },
    { "dx", parse_double, NULL, &dx },
    { "dy", parse_double, NULL, &dy },
    { "spread", parse_double, NULL, &spread },
    { "blur", parse_double, NULL, &blur }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  return gsk_inset_shadow_node_new (&outline, &color, dx, dy, spread, blur);
}

typedef union {
    gint32 i;
    double v[4];
} UniformValue;

typedef struct {
  GskGLShader *shader;
  GskShaderArgsBuilder *args;
} ShaderInfo;

static void
clear_shader_info (gpointer data)
{
  ShaderInfo *info = data;
  g_clear_object (&info->shader);
  g_clear_pointer (&info->args, gsk_shader_args_builder_unref);
}

static gboolean
parse_shader (GtkCssParser *parser,
              gpointer      out_shader_info)
{
  ShaderInfo *shader_info = out_shader_info;
  char *sourcecode = NULL;
  GBytes *bytes;
  GskGLShader *shader;

  if (!parse_string (parser, &sourcecode))
    return FALSE;

  bytes = g_bytes_new_take (sourcecode, strlen (sourcecode));
  shader = gsk_gl_shader_new_from_bytes (bytes);
  g_bytes_unref (bytes);

  shader_info->shader = shader;

  return TRUE;
}

static gboolean
parse_uniform_value (GtkCssParser *parser,
                     int           idx,
                     ShaderInfo   *shader_info)
{
  switch (gsk_gl_shader_get_uniform_type (shader_info->shader, idx))
    {
    case GSK_GL_UNIFORM_TYPE_FLOAT:
      {
        double f;
        if (!gtk_css_parser_consume_number (parser, &f))
          return FALSE;
        gsk_shader_args_builder_set_float (shader_info->args, idx, f);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_INT:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i))
          return FALSE;
        gsk_shader_args_builder_set_int (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_UINT:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i) || i < 0)
          return FALSE;
        gsk_shader_args_builder_set_uint (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_BOOL:
      {
        int i;
        if (!gtk_css_parser_consume_integer (parser, &i) || (i != 0 && i != 1))
          return FALSE;
        gsk_shader_args_builder_set_bool (shader_info->args, idx, i);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC2:
      {
        double f0, f1;
        graphene_vec2_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1))
          return FALSE;
        graphene_vec2_init (&v, f0, f1);
        gsk_shader_args_builder_set_vec2 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC3:
      {
        double f0, f1, f2;
        graphene_vec3_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1) ||
            !gtk_css_parser_consume_number (parser, &f2))
          return FALSE;
        graphene_vec3_init (&v, f0, f1, f2);
        gsk_shader_args_builder_set_vec3 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_VEC4:
      {
        double f0, f1, f2, f3;
        graphene_vec4_t v;
        if (!gtk_css_parser_consume_number (parser, &f0) ||
            !gtk_css_parser_consume_number (parser, &f1) ||
            !gtk_css_parser_consume_number (parser, &f2) ||
            !gtk_css_parser_consume_number (parser, &f3))
          return FALSE;
        graphene_vec4_init (&v, f0, f1, f2, f3);
        gsk_shader_args_builder_set_vec4 (shader_info->args, idx, &v);
      }
      break;

    case GSK_GL_UNIFORM_TYPE_NONE:
    default:
      g_assert_not_reached ();
      break;
    }

  if (idx < gsk_gl_shader_get_n_uniforms (shader_info->shader))
    {
      if (!gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA))
        return FALSE;
    }

  return TRUE;
}

static gboolean
parse_shader_args (GtkCssParser *parser, gpointer data)
{
  ShaderInfo *shader_info = data;
  int n_uniforms;
  int i;

  shader_info->args = gsk_shader_args_builder_new (shader_info->shader, NULL);
  n_uniforms = gsk_gl_shader_get_n_uniforms (shader_info->shader);

  for (i = 0; i < n_uniforms; i++)
    {
      if (!parse_uniform_value (parser, i, shader_info))
        return FALSE;
    }

  return TRUE;
}

static GskRenderNode *
parse_glshader_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child[4] = { NULL, };
  ShaderInfo shader_info = {
    NULL,
    NULL,
  };
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "sourcecode", parse_shader, NULL, &shader_info },
    { "args", parse_shader_args, clear_shader_info, &shader_info },
    { "child1", parse_node, clear_node, &child[0] },
    { "child2", parse_node, clear_node, &child[1] },
    { "child3", parse_node, clear_node, &child[2] },
    { "child4", parse_node, clear_node, &child[3] },
  };
  GskGLShader *shader;
  GskRenderNode *node;
  GBytes *args = NULL;
  int len, i;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  for (len = 0; len < 4; len++)
    {
      if (child[len] == NULL)
        break;
    }

  shader = shader_info.shader;
  args = gsk_shader_args_builder_free_to_args (shader_info.args);

  node = gsk_gl_shader_node_new (shader, &bounds, args, child, len);

  g_bytes_unref (args);
  g_object_unref (shader);

  for (i = 0; i < 4; i++)
    {
      if (child[i])
        gsk_render_node_unref (child[i]);
    }

  return node;
}

static GskRenderNode *
parse_border_node (GtkCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  float widths[4] = { 1, 1, 1, 1 };
  GdkRGBA colors[4] = { GDK_RGBA("000"), GDK_RGBA("000"), GDK_RGBA("000"), GDK_RGBA("000") };
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "widths", parse_float4, NULL, &widths },
    { "colors", parse_colors4, NULL, &colors }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  return gsk_border_node_new (&outline, widths, colors);
}

static GskRenderNode *
parse_texture_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkTexture *texture = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "texture", parse_texture, clear_texture, &texture }
  };
  GskRenderNode *node;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  if (texture == NULL)
    texture = create_default_texture ();

  node = gsk_texture_node_new (texture, &bounds);
  g_object_unref (texture);

  return node;
}

static GskRenderNode *
parse_cairo_node (GtkCssParser *parser)
{
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 50, 50);
  GdkTexture *pixels = NULL;
  cairo_surface_t *surface = NULL;
  const Declaration declarations[] = {
    { "bounds", parse_rect, NULL, &bounds },
    { "pixels", parse_texture, clear_texture, &pixels },
    { "script", parse_script, clear_surface, &surface }
  };
  GskRenderNode *node;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  node = gsk_cairo_node_new (&bounds);

  if (surface != NULL)
    {
      cairo_t *cr = gsk_cairo_node_get_draw_context (node);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else if (pixels != NULL)
    {
      cairo_t *cr = gsk_cairo_node_get_draw_context (node);
      surface = gdk_texture_download_surface (pixels);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else
    {
      /* do nothing */
    }

  g_clear_object (&pixels);
  g_clear_pointer (&surface, cairo_surface_destroy);

  return node;
}

static GskRenderNode *
parse_outset_shadow_node (GtkCssParser *parser)
{
  GskRoundedRect outline = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GdkRGBA color = GDK_RGBA("000000");
  double dx = 1, dy = 1, blur = 0, spread = 0;
  const Declaration declarations[] = {
    { "outline", parse_rounded_rect, NULL, &outline },
    { "color", parse_color, NULL, &color },
    { "dx", parse_double, NULL, &dx },
    { "dy", parse_double, NULL, &dy },
    { "spread", parse_double, NULL, &spread },
    { "blur", parse_double, NULL, &blur }
  };

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  return gsk_outset_shadow_node_new (&outline, &color, dx, dy, spread, blur);
}

static GskRenderNode *
parse_transform_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  GskTransform *transform = NULL;
  const Declaration declarations[] = {
    { "transform", parse_transform, clear_transform, &transform },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

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
  double opacity = 0.5;
  const Declaration declarations[] = {
    { "opacity", parse_double, NULL, &opacity },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

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
  graphene_vec4_t offset;
  const Declaration declarations[] = {
    { "matrix", parse_transform, clear_transform, &transform },
    { "offset", parse_vec4, NULL, &offset },
    { "child", parse_node, clear_node, &child }
  };
  GskRenderNode *result;

  graphene_vec4_init (&offset, 0, 0, 0, 0);

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

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
    { "progress", parse_double, NULL, &progress },
    { "start", parse_node, clear_node, &start },
    { "end", parse_node, clear_node, &end },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (start == NULL)
    start = gsk_color_node_new (&GDK_RGBA("AAFF00"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  if (end == NULL)
    end = create_default_render_node ();

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
    { "mode", parse_blend_mode, NULL, &mode },
    { "bottom", parse_node, clear_node, &bottom },
    { "top", parse_node, clear_node, &top },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (bottom == NULL)
    bottom = gsk_color_node_new (&GDK_RGBA("AAFF00"), &GRAPHENE_RECT_INIT (0, 0, 50, 50));
  if (top == NULL)
    top = create_default_render_node ();

  result = gsk_blend_node_new (bottom, top, mode);

  gsk_render_node_unref (bottom);
  gsk_render_node_unref (top);

  return result;
}

static GskRenderNode *
parse_repeat_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  graphene_rect_t bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  graphene_rect_t child_bounds = GRAPHENE_RECT_INIT (0, 0, 0, 0);
  const Declaration declarations[] = {
    { "child", parse_node, clear_node, &child },
    { "bounds", parse_rect, NULL, &bounds },
    { "child-bounds", parse_rect, NULL, &child_bounds },
  };
  GskRenderNode *result;
  guint parse_result;

  parse_result = parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (!(parse_result & (1 << 1)))
    gsk_render_node_get_bounds (child, &bounds);
  if (!(parse_result & (1 << 2)))
    gsk_render_node_get_bounds (child, &child_bounds);

  result = gsk_repeat_node_new (&bounds, child, &child_bounds);

  gsk_render_node_unref (child);

  return result;
}

static gboolean
unpack_glyphs (PangoFont        *font,
               PangoGlyphString *glyphs)
{
  PangoGlyphString *ascii = NULL;
  guint i;

  for (i = 0; i < glyphs->num_glyphs; i++)
    {
      PangoGlyph glyph = glyphs->glyphs[i].glyph;

      if (glyph < PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH ||
          glyph >= PANGO_GLYPH_INVALID_INPUT)
        continue;

      glyph = glyph - (PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH) - MIN_ASCII_GLYPH;

      if (ascii == NULL)
        {
          ascii = create_ascii_glyphs (font);
          if (ascii == NULL)
            return FALSE;
        }

      glyphs->glyphs[i].glyph = ascii->glyphs[glyph].glyph;
      glyphs->glyphs[i].geometry.width = ascii->glyphs[glyph].geometry.width;
    }

  g_clear_pointer (&ascii, pango_glyph_string_free);

  return TRUE;
}

static GskRenderNode *
parse_text_node (GtkCssParser *parser)
{
  PangoFont *font = NULL;
  graphene_point_t offset = GRAPHENE_POINT_INIT (0, 0);
  GdkRGBA color = GDK_RGBA("000000");
  PangoGlyphString *glyphs = NULL;
  const Declaration declarations[] = {
    { "font", parse_font, clear_font, &font },
    { "offset", parse_point, NULL, &offset },
    { "color", parse_color, NULL, &color },
    { "glyphs", parse_glyphs, clear_glyphs, &glyphs }
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));

  if (font == NULL)
    {
      font = font_from_string ("Cantarell 11");
      g_assert (font);
    }

  if (!glyphs)
    {
      const char *text = "Hello";
      PangoGlyphInfo gi = { 0, { 0, 0, 0}, { 1 } };
      guint i;

      glyphs = pango_glyph_string_new ();
      pango_glyph_string_set_size (glyphs, strlen (text));
      for (i = 0; i < strlen (text); i++)
        {
          gi.glyph = PANGO_GLYPH_INVALID_INPUT - MAX_ASCII_GLYPH + text[i];
          glyphs->glyphs[i] = gi;
        }
    }

  if (!unpack_glyphs (font, glyphs))
    {
      gtk_css_parser_error_value (parser, "Given font cannot decode the glyph text");
      result = NULL;
    }
  else
    {
      result = gsk_text_node_new (font, glyphs, &color, &offset);
      if (result == NULL)
        {
          gtk_css_parser_error_value (parser, "Glyphs result in empty text");
        }
    }

  g_object_unref (font);
  pango_glyph_string_free (glyphs);

  /* return anything, whatever, just not NULL */
  if (result == NULL)
    result = create_default_render_node ();

  return result;
}

static GskRenderNode *
parse_blur_node (GtkCssParser *parser)
{
  GskRenderNode *child = NULL;
  double blur_radius = 1.0;
  const Declaration declarations[] = {
    { "blur", parse_double, NULL, &blur_radius },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  result = gsk_blur_node_new (child, blur_radius);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_clip_node (GtkCssParser *parser)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, NULL, &clip },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (gsk_rounded_rect_is_rectilinear (&clip))
    result = gsk_clip_node_new (child, &clip.bounds);
  else
    result = gsk_rounded_clip_node_new (child, &clip);

  gsk_render_node_unref (child);

  return result;
}

static GskRenderNode *
parse_rounded_clip_node (GtkCssParser *parser)
{
  GskRoundedRect clip = GSK_ROUNDED_RECT_INIT (0, 0, 50, 50);
  GskRenderNode *child = NULL;
  const Declaration declarations[] = {
    { "clip", parse_rounded_rect, NULL, &clip },
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

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
    { "child", parse_node, clear_node, &child },
    { "shadows", parse_shadows, clear_shadows, shadows }
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

  if (shadows->len == 0)
    {
      GskShadow default_shadow = { GDK_RGBA("000000"), 1, 1, 0 };
      g_array_append_val (shadows, default_shadow);
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
    { "message", parse_string, clear_string, &message},
    { "child", parse_node, clear_node, &child },
  };
  GskRenderNode *result;

  parse_declarations (parser, declarations, G_N_ELEMENTS (declarations));
  if (child == NULL)
    child = create_default_render_node ();

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
    { "blend", parse_blend_node },
    { "blur", parse_blur_node },
    { "border", parse_border_node },
    { "cairo", parse_cairo_node },
    { "clip", parse_clip_node },
    { "color", parse_color_node },
    { "color-matrix", parse_color_matrix_node },
    { "container", parse_container_node },
    { "cross-fade", parse_cross_fade_node },
    { "debug", parse_debug_node },
    { "inset-shadow", parse_inset_shadow_node },
    { "linear-gradient", parse_linear_gradient_node },
    { "radial-gradient", parse_radial_gradient_node },
    { "conic-gradient", parse_conic_gradient_node },
    { "opacity", parse_opacity_node },
    { "outset-shadow", parse_outset_shadow_node },
    { "repeat", parse_repeat_node },
    { "repeating-linear-gradient", parse_repeating_linear_gradient_node },
    { "repeating-radial-gradient", parse_repeating_radial_gradient_node },
    { "rounded-clip", parse_rounded_clip_node },
    { "shadow", parse_shadow_node },
    { "text", parse_text_node },
    { "texture", parse_texture_node },
    { "transform", parse_transform_node },
    { "glshader", parse_glshader_node },
  };
  GskRenderNode **node_p = out_node;
  guint i;

  for (i = 0; i < G_N_ELEMENTS (node_parsers); i++)
    {
      if (gtk_css_parser_try_ident (parser, node_parsers[i].name))
        {
          GskRenderNode *node;

          if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
            {
              gtk_css_parser_error_syntax (parser, "Expected '{' after node name");
              return FALSE;
            }
          gtk_css_parser_end_block_prelude (parser);
          node = node_parsers[i].func (parser);
          if (node)
            {
              if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
                gtk_css_parser_error_syntax (parser, "Expected '}' at end of node definition");
              g_clear_pointer (node_p, gsk_render_node_unref);
              *node_p = node;
            }

          return node != NULL;
        }
    }

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    gtk_css_parser_error_value (parser, "\"%s\" is not a valid node name",
                                gtk_css_token_get_string (gtk_css_parser_get_token (parser)));
  else
    gtk_css_parser_error_syntax (parser, "Expected a node name");

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
    error_func_pair->error_func ((const GskParseLocation *)start,
                                 (const GskParseLocation *)end,
                                 error,
                                 error_func_pair->user_data);
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

  parser = gtk_css_parser_new_for_bytes (bytes, NULL, gsk_render_node_parser_error,
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
                    float       value,
                    float       default_value)
{
  /* Don't approximate-compare here, better be topo verbose */
  if (value == default_value)
    return;

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
append_string_param (Printer    *p,
                     const char *param_name,
                     const char *value)
{
  _indent (p);
  g_string_append_printf (p->str, "%s: ", param_name);
  gtk_css_print_string (p->str, value, TRUE);
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
append_stops_param (Printer            *p,
                    const char         *param_name,
                    const GskColorStop *stops,
                    gsize               n_stops)
{
  gsize i;

  _indent (p);
  g_string_append (p->str, param_name);
  g_string_append (p->str, ": ");

  for (i = 0; i < n_stops; i ++)
    {
      if (i > 0)
        g_string_append (p->str, ", ");

      string_append_double (p->str, stops[i].offset);
      g_string_append_c (p->str, ' ');
      append_rgba (p->str, &stops[i].color);
    }
  g_string_append (p->str, ";\n");
}

static cairo_status_t
cairo_write_array (void                *closure,
                   const unsigned char *data,
                   unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
cairo_destroy_array (gpointer array)
{
  g_byte_array_free (array, TRUE);
}

static void
append_escaping_newlines (GString    *str,
                          const char *string)
{
  gsize len;

  do {
    len = strcspn (string, "\n");
    g_string_append_len (str, string, len);
    string += len;
    g_string_append (str, "\\\n");
    string++;
  } while (*string);
}

/* like g_base64 encode, but breaks lines
 * in CSS-compatible way
 */
static char *
base64_encode_with_linebreaks (const guchar *data,
                               gsize         len)
{
  gsize max;
  char *out;
  int state = 0, outlen;
  int save = 0;

  g_return_val_if_fail (data != NULL || len == 0, NULL);

  /* We can use a smaller limit here, since we know the saved state is 0,
     +1 is needed for trailing \0, also check for unlikely integer overflow */
  g_return_val_if_fail (len < ((G_MAXSIZE - 1) / 4 - 1) * 3, NULL);

  max = (len / 3 + 1) * 4 + 1;
  max += 2 * (max / 76);

  out = g_malloc (max);

  outlen = g_base64_encode_step (data, len, TRUE, out, &state, &save);
  outlen += g_base64_encode_close (TRUE, out + outlen, &state, &save);
  out[outlen] = '\0';

  return out;
}

void
gsk_text_node_serialize_glyphs (GskRenderNode *node,
                                GString       *p)
{
  const guint n_glyphs = gsk_text_node_get_num_glyphs (node);
  const PangoGlyphInfo *glyphs = gsk_text_node_get_glyphs (node, NULL);
  PangoFont *font = gsk_text_node_get_font (node);
  GString *str;
  guint i, j;
  PangoGlyphString *ascii;

  ascii = create_ascii_glyphs (font);
  str = g_string_new ("");

  for (i = 0; i < n_glyphs; i++)
    {
      if (ascii)
        {
          for (j = 0; j < ascii->num_glyphs; j++)
            {
              if (glyphs[i].glyph == ascii->glyphs[j].glyph &&
                  glyphs[i].geometry.width == ascii->glyphs[j].geometry.width &&
                  glyphs[i].geometry.x_offset == 0 &&
                  glyphs[i].geometry.y_offset == 0 &&
                  glyphs[i].attr.is_cluster_start &&
                  !glyphs[i].attr.is_color)
                {
                  switch (j + MIN_ASCII_GLYPH)
                    {
                      case '\\':
                        g_string_append (str, "\\\\");
                        break;
                      case '"':
                        g_string_append (str, "\\\"");
                        break;
                      default:
                        g_string_append_c (str, j + MIN_ASCII_GLYPH);
                        break;
                    }
                  break;
                }
            }
          if (j != ascii->num_glyphs)
            continue;
        }

      if (str->len)
        {
          g_string_append_printf (p, "\"%s\", ", str->str);
          g_string_set_size (str, 0);
        }

      g_string_append_printf (p, "%u ", glyphs[i].glyph);
      string_append_double (p, (double) glyphs[i].geometry.width / PANGO_SCALE);
      if (!glyphs[i].attr.is_cluster_start ||
          glyphs[i].attr.is_color ||
          glyphs[i].geometry.x_offset != 0 ||
          glyphs[i].geometry.y_offset != 0)
        {
          g_string_append (p, " ");
          string_append_double (p, (double) glyphs[i].geometry.x_offset / PANGO_SCALE);
          g_string_append (p, " ");
          string_append_double (p, (double) glyphs[i].geometry.y_offset / PANGO_SCALE);
          if (!glyphs[i].attr.is_cluster_start)
            g_string_append (p, " same-cluster");
          if (!glyphs[i].attr.is_color)
            g_string_append (p, " color");
        }

      if (i + 1 < n_glyphs)
        g_string_append (p, ", ");
    }

  if (str->len)
    g_string_append_printf (p, "\"%s\"", str->str);

  g_string_free (str, TRUE);
  if (ascii)
    pango_glyph_string_free (ascii);
}

static void
render_node_print (Printer       *p,
                   GskRenderNode *node)
{
  char *b64;

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
        append_rgba_param (p, "color", gsk_color_node_get_color (node));
        end_node (p);
      }
      break;

    case GSK_CROSS_FADE_NODE:
      {
        start_node (p, "cross-fade");

        append_float_param (p, "progress", gsk_cross_fade_node_get_progress (node), 0.5f);
        append_node_param (p, "start", gsk_cross_fade_node_get_start_child (node));
        append_node_param (p, "end", gsk_cross_fade_node_get_end_child (node));

        end_node (p);
      }
      break;

    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
      {
        if (gsk_render_node_get_node_type (node) == GSK_REPEATING_LINEAR_GRADIENT_NODE)
          start_node (p, "repeating-linear-gradient");
        else
          start_node (p, "linear-gradient");

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "start", gsk_linear_gradient_node_get_start (node));
        append_point_param (p, "end", gsk_linear_gradient_node_get_end (node));
        append_stops_param (p, "stops", gsk_linear_gradient_node_get_color_stops (node, NULL),
                                        gsk_linear_gradient_node_get_n_color_stops (node));

        end_node (p);
      }
      break;

    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
      {
        if (gsk_render_node_get_node_type (node) == GSK_REPEATING_RADIAL_GRADIENT_NODE)
          start_node (p, "repeating-radial-gradient");
        else
          start_node (p, "radial-gradient");

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "center", gsk_radial_gradient_node_get_center (node));
        append_float_param (p, "hradius", gsk_radial_gradient_node_get_hradius (node), 0.0f);
        append_float_param (p, "vradius", gsk_radial_gradient_node_get_vradius (node), 0.0f);
        append_float_param (p, "start", gsk_radial_gradient_node_get_start (node), 0.0f);
        append_float_param (p, "end", gsk_radial_gradient_node_get_end (node), 1.0f);

        append_stops_param (p, "stops", gsk_radial_gradient_node_get_color_stops (node, NULL),
                                        gsk_radial_gradient_node_get_n_color_stops (node));

        end_node (p);
      }
      break;

    case GSK_CONIC_GRADIENT_NODE:
      {
        start_node (p, "conic-gradient");

        append_rect_param (p, "bounds", &node->bounds);
        append_point_param (p, "center", gsk_conic_gradient_node_get_center (node));
        append_float_param (p, "rotation", gsk_conic_gradient_node_get_rotation (node), 0.0f);

        append_stops_param (p, "stops", gsk_conic_gradient_node_get_color_stops (node, NULL),
                                        gsk_conic_gradient_node_get_n_color_stops (node));

        end_node (p);
      }
      break;

    case GSK_OPACITY_NODE:
      {
        start_node (p, "opacity");

        append_float_param (p, "opacity", gsk_opacity_node_get_opacity (node), 0.5f);
        append_node_param (p, "child", gsk_opacity_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_OUTSET_SHADOW_NODE:
      {
        const GdkRGBA *color = gsk_outset_shadow_node_get_color (node);

        start_node (p, "outset-shadow");

        append_float_param (p, "blur", gsk_outset_shadow_node_get_blur_radius (node), 0.0f);
        if (!gdk_rgba_equal (color, &GDK_RGBA("000")))
          append_rgba_param (p, "color", color);
        append_float_param (p, "dx", gsk_outset_shadow_node_get_dx (node), 1.0f);
        append_float_param (p, "dy", gsk_outset_shadow_node_get_dy (node), 1.0f);
        append_rounded_rect_param (p, "outline", gsk_outset_shadow_node_get_outline (node));
        append_float_param (p, "spread", gsk_outset_shadow_node_get_spread (node), 0.0f);

        end_node (p);
      }
      break;

    case GSK_CLIP_NODE:
      {
        start_node (p, "clip");

        append_rect_param (p, "clip", gsk_clip_node_get_clip (node));
        append_node_param (p, "child", gsk_clip_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_ROUNDED_CLIP_NODE:
      {
        start_node (p, "rounded-clip");

        append_rounded_rect_param (p, "clip", gsk_rounded_clip_node_get_clip (node));
        append_node_param (p, "child", gsk_rounded_clip_node_get_child (node));


        end_node (p);
      }
      break;

    case GSK_TRANSFORM_NODE:
      {
        GskTransform *transform = gsk_transform_node_get_transform (node);
        start_node (p, "transform");

        if (gsk_transform_get_category (transform) != GSK_TRANSFORM_CATEGORY_IDENTITY)
          append_transform_param (p, "transform", transform);
        append_node_param (p, "child", gsk_transform_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_COLOR_MATRIX_NODE:
      {
        start_node (p, "color-matrix");

        if (!graphene_matrix_is_identity (gsk_color_matrix_node_get_color_matrix (node)))
          append_matrix_param (p, "matrix", gsk_color_matrix_node_get_color_matrix (node));
        if (!graphene_vec4_equal (gsk_color_matrix_node_get_color_offset (node), graphene_vec4_zero ()))
          append_vec4_param (p, "offset", gsk_color_matrix_node_get_color_offset (node));
        append_node_param (p, "child", gsk_color_matrix_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BORDER_NODE:
      {
        const GdkRGBA *colors = gsk_border_node_get_colors (node);
        const float *widths = gsk_border_node_get_widths (node);
        guint i, n;
        start_node (p, "border");

        if (!gdk_rgba_equal (&colors[3], &colors[1]))
          n = 4;
        else if (!gdk_rgba_equal (&colors[2], &colors[0]))
          n = 3;
        else if (!gdk_rgba_equal (&colors[1], &colors[0]))
          n = 2;
        else if (!gdk_rgba_equal (&colors[0], &GDK_RGBA("000000")))
          n = 1;
        else
          n = 0;

        if (n > 0)
          {
            _indent (p);
            g_string_append (p->str, "colors: ");
            for (i = 0; i < n; i++)
              {
                if (i > 0)
                  g_string_append_c (p->str, ' ');
                append_rgba (p->str, &colors[i]);
              }
            g_string_append (p->str, ";\n");
          }

        append_rounded_rect_param (p, "outline", gsk_border_node_get_outline (node));

        if (widths[3] != widths[1])
          n = 4;
        else if (widths[2] != widths[0])
          n = 3;
        else if (widths[1] != widths[0])
          n = 2;
        else if (widths[0] != 1.0)
          n = 1;
        else
          n = 0;

        if (n > 0)
          {
            _indent (p);
            g_string_append (p->str, "widths: ");
            for (i = 0; i < n; i++)
              {
                if (i > 0)
                  g_string_append_c (p->str, ' ');
                string_append_double (p->str, widths[i]);
              }
            g_string_append (p->str, ";\n");
          }

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
            const GskShadow *s = gsk_shadow_node_get_shadow (node, i);
            char *color;

            if (i > 0)
              g_string_append (p->str, ", ");

            color = gdk_rgba_to_string (&s->color);
            g_string_append (p->str, color);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->dx);
            g_string_append_c (p->str, ' ');
            string_append_double (p->str, s->dy);
            if (s->radius > 0)
              {
                g_string_append_c (p->str, ' ');
                string_append_double (p->str, s->radius);
              }

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
        const GdkRGBA *color = gsk_inset_shadow_node_get_color (node);
        start_node (p, "inset-shadow");

        append_float_param (p, "blur", gsk_inset_shadow_node_get_blur_radius (node), 0.0f);
        if (!gdk_rgba_equal (color, &GDK_RGBA("000")))
          append_rgba_param (p, "color", color);
        append_float_param (p, "dx", gsk_inset_shadow_node_get_dx (node), 1.0f);
        append_float_param (p, "dy", gsk_inset_shadow_node_get_dy (node), 1.0f);
        append_rounded_rect_param (p, "outline", gsk_inset_shadow_node_get_outline (node));
        append_float_param (p, "spread", gsk_inset_shadow_node_get_spread (node), 0.0f);

        end_node (p);
      }
      break;

    case GSK_TEXTURE_NODE:
      {
        GdkTexture *texture = gsk_texture_node_get_texture (node);
        GBytes *bytes;

        start_node (p, "texture");
        append_rect_param (p, "bounds", &node->bounds);

        _indent (p);

        switch (gdk_texture_get_format (texture))
          {
          case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
          case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
          case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
          case GDK_MEMORY_B8G8R8A8:
          case GDK_MEMORY_A8R8G8B8:
          case GDK_MEMORY_R8G8B8A8:
          case GDK_MEMORY_A8B8G8R8:
          case GDK_MEMORY_R8G8B8:
          case GDK_MEMORY_B8G8R8:
          case GDK_MEMORY_R16G16B16:
          case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
          case GDK_MEMORY_R16G16B16A16:
            bytes = gdk_texture_save_to_png_bytes (texture);
            g_string_append (p->str, "texture: url(\"data:image/png;base64,");
            break;

          case GDK_MEMORY_R16G16B16_FLOAT:
          case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
          case GDK_MEMORY_R16G16B16A16_FLOAT:
          case GDK_MEMORY_R32G32B32_FLOAT:
          case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
          case GDK_MEMORY_R32G32B32A32_FLOAT:
            bytes = gdk_texture_save_to_tiff_bytes (texture);
            g_string_append (p->str, "texture: url(\"data:image/tiff;base64,");
            break;

          case GDK_MEMORY_N_FORMATS:
          default:
            g_assert_not_reached ();
          }

        b64 = base64_encode_with_linebreaks (g_bytes_get_data (bytes, NULL),
                                             g_bytes_get_size (bytes));
        append_escaping_newlines (p->str, b64);
        g_free (b64);
        g_string_append (p->str, "\");\n");
        end_node (p);

        g_bytes_unref (bytes);
      }
      break;

    case GSK_TEXT_NODE:
      {
        const graphene_point_t *offset = gsk_text_node_get_offset (node);
        const GdkRGBA *color = gsk_text_node_get_color (node);
        PangoFont *font = gsk_text_node_get_font (node);
        PangoFontDescription *desc;
        char *font_name;

        start_node (p, "text");

        if (!gdk_rgba_equal (color, &GDK_RGBA("000000")))
          append_rgba_param (p, "color", color);

        _indent (p);
        desc = pango_font_describe (font);
        font_name = pango_font_description_to_string (desc);
        g_string_append_printf (p->str, "font: \"%s\";\n", font_name);
        g_free (font_name);
        pango_font_description_free (desc);

        _indent (p);
        g_string_append (p->str, "glyphs: ");

        gsk_text_node_serialize_glyphs (node, p->str);

        g_string_append_c (p->str, ';');
        g_string_append_c (p->str, '\n');

        if (!graphene_point_equal (offset, graphene_point_zero ()))
          append_point_param (p, "offset", offset);

        end_node (p);
      }
      break;

    case GSK_DEBUG_NODE:
      {
        const char *message = gsk_debug_node_get_message (node);

        start_node (p, "debug");

        /* TODO: We potentially need to escape certain characters in the message */
        if (message)
          {
            _indent (p);
            g_string_append_printf (p->str, "message: \"%s\";\n", message);
          }
        append_node_param (p, "child", gsk_debug_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BLUR_NODE:
      {
        start_node (p, "blur");

        append_float_param (p, "blur", gsk_blur_node_get_radius (node), 1.0f);
        append_node_param (p, "child", gsk_blur_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_GL_SHADER_NODE:
      {
        GskGLShader *shader = gsk_gl_shader_node_get_shader (node);
        GBytes *args = gsk_gl_shader_node_get_args (node);

        start_node (p, "glshader");

        append_rect_param (p, "bounds", &node->bounds);

        GBytes *bytes = gsk_gl_shader_get_source (shader);
        /* Ensure we are zero-terminated */
        char *sourcecode = g_strndup (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
        append_string_param (p, "sourcecode", sourcecode);
        g_free (sourcecode);

        if (gsk_gl_shader_get_n_uniforms (shader) > 0)
          {
            GString *data = g_string_new ("");

            for (guint i = 0; i < gsk_gl_shader_get_n_uniforms (shader); i++)
              {
                if (i > 0)
                  g_string_append (data, ", ");

                switch (gsk_gl_shader_get_uniform_type (shader, i))
                  {
                  case GSK_GL_UNIFORM_TYPE_NONE:
                  default:
                    g_assert_not_reached ();
                    break;

                  case GSK_GL_UNIFORM_TYPE_FLOAT:
                    {
                      float value = gsk_gl_shader_get_arg_float (shader, args, i);
                      string_append_double (data, value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_INT:
                    {
                      gint32 value = gsk_gl_shader_get_arg_int (shader, args, i);
                      g_string_append_printf (data, "%d", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_UINT:
                    {
                      guint32 value = gsk_gl_shader_get_arg_uint (shader, args, i);
                      g_string_append_printf (data, "%u", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_BOOL:
                    {
                      gboolean value = gsk_gl_shader_get_arg_bool (shader, args, i);
                      g_string_append_printf (data, "%d", value);
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC2:
                    {
                      graphene_vec2_t value;
                      gsk_gl_shader_get_arg_vec2 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec2_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec2_get_y (&value));
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC3:
                    {
                      graphene_vec3_t value;
                      gsk_gl_shader_get_arg_vec3 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec3_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec3_get_y (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec3_get_z (&value));
                    }
                    break;

                  case GSK_GL_UNIFORM_TYPE_VEC4:
                    {
                      graphene_vec4_t value;
                      gsk_gl_shader_get_arg_vec4 (shader, args, i,
                                                  &value);
                      string_append_double (data, graphene_vec4_get_x (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_y (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_z (&value));
                      g_string_append (data, " ");
                      string_append_double (data, graphene_vec4_get_w (&value));
                    }
                    break;
                  }
              }
            _indent (p);
            g_string_append_printf (p->str, "args: %s;\n", data->str);
            g_string_free (data, TRUE);
          }

        for (guint i = 0; i < gsk_gl_shader_node_get_n_children (node); i ++)
          {
            GskRenderNode *child = gsk_gl_shader_node_get_child (node, i);
            char *name;

            name = g_strdup_printf ("child%d", i + 1);
            append_node_param (p, name, child);
            g_free (name);
          }

        end_node (p);
      }
      break;

    case GSK_REPEAT_NODE:
      {
        GskRenderNode *child = gsk_repeat_node_get_child (node);
        const graphene_rect_t *child_bounds = gsk_repeat_node_get_child_bounds (node);

        start_node (p, "repeat");

        if (!graphene_rect_equal (&node->bounds, &child->bounds))
          append_rect_param (p, "bounds", &node->bounds);
        if (!graphene_rect_equal (child_bounds, &child->bounds))
          append_rect_param (p, "child-bounds", child_bounds);
        append_node_param (p, "child", gsk_repeat_node_get_child (node));

        end_node (p);
      }
      break;

    case GSK_BLEND_NODE:
      {
        GskBlendMode mode = gsk_blend_node_get_blend_mode (node);
        guint i;

        start_node (p, "blend");

        if (mode != GSK_BLEND_MODE_DEFAULT)
          {
            _indent (p);
            for (i = 0; i < G_N_ELEMENTS (blend_modes); i++)
              {
                if (blend_modes[i].mode == mode)
                  {
                    g_string_append_printf (p->str, "mode: %s;\n", blend_modes[i].name);
                    break;
                  }
              }
          }
        append_node_param (p, "bottom", gsk_blend_node_get_bottom_child (node));
        append_node_param (p, "top", gsk_blend_node_get_top_child (node));

        end_node (p);
      }
      break;

    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    case GSK_CAIRO_NODE:
      {
        cairo_surface_t *surface = gsk_cairo_node_get_surface (node);
        GByteArray *array;

        start_node (p, "cairo");
        append_rect_param (p, "bounds", &node->bounds);

        if (surface != NULL)
          {
            array = g_byte_array_new ();
            cairo_surface_write_to_png_stream (surface, cairo_write_array, array);

            _indent (p);
            g_string_append (p->str, "pixels: url(\"data:image/png;base64,");
            b64 = base64_encode_with_linebreaks (array->data, array->len);
            append_escaping_newlines (p->str, b64);
            g_free (b64);
            g_string_append (p->str, "\");\n");

            g_byte_array_free (array, TRUE);

#ifdef CAIRO_HAS_SCRIPT_SURFACE
            if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_RECORDING)
              {
                static const cairo_user_data_key_t cairo_is_stupid_key;
                cairo_device_t *script;

                array = g_byte_array_new ();
                script = cairo_script_create_for_stream (cairo_write_array, array);

                if (cairo_script_from_recording_surface (script, surface) == CAIRO_STATUS_SUCCESS)
                  {
                    _indent (p);
                    g_string_append (p->str, "script: url(\"data:;base64,");
                    b64 = base64_encode_with_linebreaks (array->data, array->len);
                    append_escaping_newlines (p->str, b64);
                    g_free (b64);
                    g_string_append (p->str, "\");\n");
                  }

                /* because Cairo is stupid and writes to the device after we finished it,
                 * we can't just
                g_byte_array_free (array, TRUE);
                 * but have to
                 */
                g_byte_array_set_size (array, 0);
                cairo_device_set_user_data (script, &cairo_is_stupid_key, array, cairo_destroy_array);
                cairo_device_destroy (script);
              }
#endif
          }

        end_node (p);
      }
      break;

    case GSK_GLYPH_NODE:

    default:
      g_error ("Unhandled node: %s", g_type_name_from_instance ((GTypeInstance *) node));
      break;
    }
}

/**
 * gsk_render_node_serialize:
 * @node: a `GskRenderNode`
 *
 * Serializes the @node for later deserialization via
 * gsk_render_node_deserialize(). No guarantees are made about the format
 * used other than that the same version of GTK will be able to deserialize
 * the result of a call to gsk_render_node_serialize() and
 * gsk_render_node_deserialize() will correctly reject files it cannot open
 * that were created with previous versions of GTK.
 *
 * The intended use of this functions is testing, benchmarking and debugging.
 * The format is not meant as a permanent storage format.
 *
 * Returns: a `GBytes` representing the node.
 **/
GBytes *
gsk_render_node_serialize (GskRenderNode *node)
{
  Printer p;

  printer_init (&p);

  if (gsk_render_node_get_node_type (node) == GSK_CONTAINER_NODE)
    {
      guint i;

      for (i = 0; i < gsk_container_node_get_n_children (node); i ++)
        {
          GskRenderNode *child = gsk_container_node_get_child (node, i);

          render_node_print (&p, child);
        }
    }
  else
    {
      render_node_print (&p, node);
    }

  return g_string_free_to_bytes (p.str);
}
