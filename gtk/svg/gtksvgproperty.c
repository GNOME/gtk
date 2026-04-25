#include "config.h"

#include "gtksvgpropertyprivate.h"

#include "gtksvgelementtypeprivate.h"
#include "gtksvgutilsprivate.h"

#include "gtksvgclipprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgcontentfitprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtksvgfilterrefprivate.h"
#include "gtksvgfilterfunctionsprivate.h"
#include "gtksvghrefprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvgmaskprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgorientprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgpathdataprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvgstringprivate.h"
#include "gtksvgtextdecorationprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgviewboxprivate.h"

static SvgValue *
parse_points (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_numbers_new_none ();
  else
    {
      SvgValue *p = svg_numbers_parse2 (parser, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH);

      if (p != NULL && svg_numbers_get_length (p) % 2 == 1)
        {
          gtk_css_parser_error_syntax (parser, "Odd number of coordinates");
          p = svg_numbers_drop_last (p);
        }

      return p;
    }
}

static SvgValue *
parse_language (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token;
      PangoLanguage *lang;

      token = gtk_css_parser_get_token (parser);
      lang = pango_language_from_string (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      if (lang)
        return svg_language_new (lang);
    }

  return NULL;
}

static SvgValue *
parse_custom_ident (GtkCssParser *parser)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      const char *string = gtk_css_token_get_string (token);
      SvgValue *value;

      if (strcmp (string, "initial") == 0 ||
          strcmp (string, "inherited") == 0)
        {
          gtk_css_parser_error_value (parser, "Can't use %s here", string);
          return NULL;
        }

      value = svg_string_new (string);

      gtk_css_parser_skip (parser);

      return value;
    }

  gtk_css_parser_error_syntax (parser, "Expected an identifier");
  return NULL;
}

static SvgValue *
parse_language_list (GtkCssParser *parser)
{
  GPtrArray *langs;
  SvgValue *value;

  langs = g_ptr_array_new ();

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      const GtkCssToken *token;
      PangoLanguage *lang;

      gtk_css_parser_skip_whitespace (parser);

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          g_ptr_array_unref (langs);
          return NULL;
        }

      token = gtk_css_parser_get_token (parser);
      lang = pango_language_from_string (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      if (!lang)
        {
          g_ptr_array_unref (langs);
          return NULL;
        }
      g_ptr_array_add (langs, lang);

      skip_whitespace_and_optional_comma (parser);
    }

  value = svg_language_new_list (langs->len, (PangoLanguage **) langs->pdata);
  g_ptr_array_unref (langs);

  return value;
}

static SvgValue *
parse_path (const char  *string,
            GError     **error)
{
  if (strcmp (string, "none") == 0)
    return svg_path_new_none ();
  else
    {
      SvgPathData *data = NULL;

      if (!svg_path_data_parse_full (string, &data))
        g_set_error (error,
                     GTK_CSS_PARSER_ERROR, GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                     "Path data is invalid");

      return svg_path_new_from_data (data);
    }
}

static SvgValue *
parse_href (const char  *string,
            GError     **error)
{
   return svg_href_new_plain (string);
}

static SvgValue *
parse_string_list (const char  *value,
                   GError     **error)
{
  return svg_string_list_new_take (parse_strv (value), ' ');
}

static SvgValue *
parse_opacity (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE);
}

static SvgValue *
parse_stroke_width (GtkCssParser *parser)
{
  return svg_number_parse (parser, 0, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_LENGTH|SVG_PARSE_PERCENTAGE);
}

static SvgValue *
parse_miterlimit (GtkCssParser *parser)
{
  return svg_number_parse (parser, 0, DBL_MAX, SVG_PARSE_NUMBER);
}

static SvgValue *
parse_any_length (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_length_percentage (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_any_number (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER);
}

static SvgValue *
parse_font_weight (GtkCssParser *parser)
{
  SvgValue *v;

  v = svg_font_weight_try_parse (parser);
  if (v)
    return v;

  return svg_number_parse (parser, 1, 1000, SVG_PARSE_NUMBER);
}

static SvgValue *
parse_font_size (GtkCssParser *parser)
{
  SvgValue *v;

  v = svg_font_size_try_parse (parser);
  if (v)
    return v;

  return svg_number_parse (parser, 0, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_letter_spacing (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "normal"))
    return svg_number_new (0.);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_offset (GtkCssParser *parser)
{
  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE);
}

static SvgValue *
parse_ref_x (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "left"))
    return svg_percentage_new (0);
  else if (gtk_css_parser_try_ident (parser, "center"))
    return svg_percentage_new (50);
  else if (gtk_css_parser_try_ident (parser, "right"))
    return svg_percentage_new (100);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_ref_y (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "top"))
    return svg_percentage_new (0);
  else if (gtk_css_parser_try_ident (parser, "center"))
    return svg_percentage_new (50);
  else if (gtk_css_parser_try_ident (parser, "bottom"))
    return svg_percentage_new (100);

  return svg_number_parse (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH);
}

static SvgValue *
parse_length_percentage_or_auto (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "auto"))
    return svg_auto_new ();

  return parse_length_percentage (parser);
}

static SvgValue *
parse_number_optional_number (GtkCssParser *parser)
{
  SvgValue *values;

  values = svg_numbers_parse (parser);

  if (values == NULL)
    {
      return NULL;
    }
  else if (svg_numbers_get_length (values) <= 2)
    {
      return values;
    }
  else
    {
      svg_value_unref (values);
      return NULL;
    }
}

static SvgValue *
parse_transform_origin (GtkCssParser *parser)
{
  double d[2];
  SvgUnit u[2];
  enum { HORIZONTAL, VERTICAL, NEUTRAL, };
  unsigned int set[2];
  unsigned int i = 0;

  d[0] = d[1] = 50;
  u[0] = u[1] = SVG_UNIT_PERCENTAGE;
  set[0] = set[1] = NEUTRAL;

  for (i = 0; i < 2; i++)
    {
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        break;

      if (gtk_css_parser_try_ident (parser, "left"))
        {
          set[i] = HORIZONTAL;
          d[i] = 0;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "right"))
        {
          set[i] = HORIZONTAL;
          d[i] = 100;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "top"))
        {
          set[i] = VERTICAL;
          d[i] = 0;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "bottom"))
        {
          set[i] = VERTICAL;
          d[i] = 100;
          u[i] = SVG_UNIT_PERCENTAGE;
        }
      else if (gtk_css_parser_try_ident (parser, "center"))
        {
          /* nothing to do */
        }
      else if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_LENGTH|SVG_PARSE_PERCENTAGE, &d[i], &u[i]))
        {
          return NULL;
        }
    }

  if (set[0] == set[1] && set[0] != NEUTRAL)
    return NULL;

  if (set[0] == HORIZONTAL)
    return svg_numbers_new2 (d[0], u[0], d[1], u[1]);
  else
    return svg_numbers_new2 (d[1], u[1], d[0], u[0]);
}

static SvgValue *
parse_font_family (GtkCssParser *parser)
{
  GStrvBuilder *builder;
  const GtkCssToken *token;
  SvgValue *result;

  builder = g_strv_builder_new ();

  while (1)
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
        {
          GString *string = g_string_new (NULL);
          token = gtk_css_parser_get_token (parser);

          g_string_append (string, gtk_css_token_get_string (token));
          gtk_css_parser_skip (parser);

          gtk_css_parser_skip_whitespace (parser);
          while (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
            {
              token = gtk_css_parser_get_token (parser);
              g_string_append_c (string, ' ');
              g_string_append (string, gtk_css_token_get_string (token));
              gtk_css_parser_skip (parser);
            }
          g_strv_builder_take (builder, g_string_free (string, FALSE));
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
        {
          token = gtk_css_parser_get_token (parser);
          g_strv_builder_add (builder, gtk_css_token_get_string (token));
          gtk_css_parser_skip (parser);
        }
      else if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
        {
          gtk_css_parser_skip (parser);
        }
      else
        break;
    }

  result = svg_string_list_new_take (g_strv_builder_unref_to_strv (builder), ',');

  return result;
}

static SvgValue *
marker_ref_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      return svg_href_new_none ();
    }
  else
    {
      char *url = gtk_css_parser_consume_url (parser);

      if (url)
        return svg_href_new_url_take (url);
    }

  gtk_css_parser_error_syntax (parser, "Expected a marker reference");
  return NULL;
}

static gboolean
number_is_positive_or_auto (const SvgValue *value)
{
  return svg_value_equal (value, svg_auto_new ()) ||
         svg_value_is_positive_number (value);
}

typedef enum
{
  SVG_PROPERTY_IS_INHERITED     = 1 << 0,
  SVG_PROPERTY_IS_DISCRETE      = 1 << 1,
  SVG_PROPERTY_NO_PRESENTATION  = 1 << 2,
  SVG_PROPERTY_NO_CSS           = 1 << 3,
} SvgPropertyFlags;

typedef struct
{
  SvgPropertyFlags flags;
  unsigned int applies_to;
  SvgValue * (* parse_value)        (GtkCssParser    *parser);
  SvgValue * (* parse_presentation) (const char      *string,
                                     GError         **error);
  gboolean   (* is_valid)           (const SvgValue  *value);
  SvgValue *initial_value;
} SvgPropertyInfo;

#define ELEMENT_ANY \
  (BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_IMAGE) | \
   BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_PATH) | BIT (SVG_ELEMENT_POLYGON) | \
   BIT (SVG_ELEMENT_POLYLINE) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_TEXT) | \
   BIT (SVG_ELEMENT_TSPAN) | \
   BIT (SVG_ELEMENT_CLIP_PATH) | BIT (SVG_ELEMENT_DEFS) | BIT (SVG_ELEMENT_GROUP) | \
   BIT (SVG_ELEMENT_MARKER) | BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_PATTERN) | \
   BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | \
   BIT (SVG_ELEMENT_LINEAR_GRADIENT) | BIT (SVG_ELEMENT_RADIAL_GRADIENT) | \
   BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_SWITCH) | \
   BIT (SVG_ELEMENT_LINK))

#define ELEMENT_SHAPES \
  (BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_LINE) | \
   BIT (SVG_ELEMENT_PATH) | BIT (SVG_ELEMENT_POLYGON) | BIT (SVG_ELEMENT_POLYLINE) | \
   BIT (SVG_ELEMENT_RECT))

#define ELEMENT_TEXTS (BIT (SVG_ELEMENT_TEXT) | BIT (SVG_ELEMENT_TSPAN))

#define ELEMENT_GRAPHICS (ELEMENT_SHAPES | ELEMENT_TEXTS | BIT (SVG_ELEMENT_IMAGE))

#define ELEMENT_GRAPHICS_REF (BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_IMAGE))

#define ELEMENT_CONTAINERS \
  (BIT (SVG_ELEMENT_CLIP_PATH) | BIT (SVG_ELEMENT_DEFS) | BIT (SVG_ELEMENT_GROUP) | \
   BIT (SVG_ELEMENT_MARKER) | BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_PATTERN) | \
   BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_SWITCH) | \
   BIT (SVG_ELEMENT_LINK))

#define ELEMENT_NEVER_RENDERED \
  (BIT (SVG_ELEMENT_CLIP_PATH) | BIT (SVG_ELEMENT_DEFS) | BIT (SVG_ELEMENT_LINEAR_GRADIENT) | \
   BIT (SVG_ELEMENT_MARKER) | BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_PATTERN) | \
   BIT (SVG_ELEMENT_RADIAL_GRADIENT) | BIT (SVG_ELEMENT_SYMBOL))

#define ELEMENT_RENDERABLE ((ELEMENT_GRAPHICS | ELEMENT_GRAPHICS_REF | ELEMENT_CONTAINERS) & ~ELEMENT_NEVER_RENDERED)

#define ELEMENT_GRADIENTS \
  (BIT (SVG_ELEMENT_LINEAR_GRADIENT) | BIT (SVG_ELEMENT_RADIAL_GRADIENT))

#define ELEMENT_PAINT_SERVERS (ELEMENT_GRADIENTS | BIT (SVG_ELEMENT_PATTERN))

#define ELEMENT_VIEWPORTS (BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL))

static SvgPropertyInfo shape_attrs[] = {
  [SVG_PROPERTY_DISPLAY] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_display_parse,
  },
  [SVG_PROPERTY_FONT_SIZE] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_font_size,
  },
  [SVG_PROPERTY_VISIBILITY] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_visibility_parse,
  },
  [SVG_PROPERTY_TRANSFORM] = {
    .applies_to = (ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN),
    .parse_value = svg_transform_parse_css,
  },
  [SVG_PROPERTY_TRANSFORM_ORIGIN] = {
    .applies_to = (ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN),
    .parse_value = parse_transform_origin,
  },
  [SVG_PROPERTY_TRANSFORM_BOX] = {
    .applies_to = (ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN),
    .parse_value = svg_transform_box_parse,
  },
  [SVG_PROPERTY_OPACITY] = {
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_opacity,
  },
  [SVG_PROPERTY_COLOR] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_color_parse,
  },
  [SVG_PROPERTY_COLOR_INTERPOLATION] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRADIENTS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_color_interpolation_parse,
  },
  [SVG_PROPERTY_COLOR_INTERPOLATION_FILTERS] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_color_interpolation_parse,
  },
  [SVG_PROPERTY_CLIP_PATH] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_clip_parse,
  },
  [SVG_PROPERTY_CLIP_RULE] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to =ELEMENT_GRAPHICS,
    .parse_value = svg_fill_rule_parse,
  },
  [SVG_PROPERTY_MASK] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_mask_parse,
  },
  [SVG_PROPERTY_FONT_FAMILY] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_font_family,
  },
  [SVG_PROPERTY_FONT_STYLE] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_font_style_parse,
  },
  [SVG_PROPERTY_FONT_VARIANT] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_font_variant_parse,
  },
  [SVG_PROPERTY_FONT_WEIGHT] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_font_weight,
  },
  [SVG_PROPERTY_FONT_STRETCH] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_font_stretch_parse,
  },
  [SVG_PROPERTY_FILTER] = {
    .applies_to = (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_filter_functions_parse_css,
  },
  [SVG_PROPERTY_FILL] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_paint_parse,
  },
  [SVG_PROPERTY_FILL_OPACITY] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = parse_opacity,
  },
  [SVG_PROPERTY_FILL_RULE] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_fill_rule_parse,
  },
  [SVG_PROPERTY_STROKE] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_paint_parse,
  },
  [SVG_PROPERTY_STROKE_OPACITY] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = parse_opacity,
  },
  [SVG_PROPERTY_STROKE_WIDTH] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = parse_stroke_width,
  },
  [SVG_PROPERTY_STROKE_LINECAP] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_linecap_parse,
  },
  [SVG_PROPERTY_STROKE_LINEJOIN] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_linejoin_parse,
  },
  [SVG_PROPERTY_STROKE_MITERLIMIT] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = parse_miterlimit,
  },
  [SVG_PROPERTY_STROKE_DASHARRAY] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_dash_array_parse,
  },
  [SVG_PROPERTY_STROKE_DASHOFFSET] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_MARKER_START] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = marker_ref_parse,
  },
  [SVG_PROPERTY_MARKER_MID] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = marker_ref_parse,
  },
  [SVG_PROPERTY_MARKER_END] = {
    .flags = SVG_PROPERTY_IS_INHERITED,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = marker_ref_parse,
  },
  [SVG_PROPERTY_PAINT_ORDER] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_SHAPES | ELEMENT_TEXTS,
    .parse_value = svg_paint_order_parse,
  },
  [SVG_PROPERTY_SHAPE_RENDERING] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = svg_shape_rendering_parse,
  },
  [SVG_PROPERTY_TEXT_RENDERING] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_text_rendering_parse,
  },
  [SVG_PROPERTY_IMAGE_RENDERING] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = BIT (SVG_ELEMENT_IMAGE),
    .parse_value = svg_image_rendering_parse,
  },
  [SVG_PROPERTY_BLEND_MODE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_PRESENTATION,
    .applies_to = ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRAPHICS_REF,
    .parse_value = svg_blend_mode_parse,
  },
  [SVG_PROPERTY_ISOLATION] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_PRESENTATION,
    .applies_to = ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRAPHICS_REF,
    .parse_value = svg_isolation_parse,
  },
  [SVG_PROPERTY_PATH_LENGTH] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = parse_any_length,
    .is_valid = svg_value_is_positive_number,
  },
  [SVG_PROPERTY_HREF] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_GRAPHICS_REF | ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_LINK),
    .parse_presentation = parse_href,
  },
  [SVG_PROPERTY_OVERFLOW] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_ANY,
    .parse_value = svg_overflow_parse,
  },
  [SVG_PROPERTY_VECTOR_EFFECT] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_vector_effect_parse,
  },
  [SVG_PROPERTY_PATH] = {
    .applies_to = BIT (SVG_ELEMENT_PATH),
    .parse_value = svg_path_parse,
    .parse_presentation = parse_path,
  },
  [SVG_PROPERTY_CX] = {
    .applies_to = BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_CY] = {
    .applies_to = BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_R] = {
    .applies_to = BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SVG_PROPERTY_X] = {
    .applies_to = BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
              BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_PATTERN) |
              BIT (SVG_ELEMENT_MASK) | ELEMENT_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_Y] = {
    .applies_to = BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
              BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_PATTERN) |
              BIT (SVG_ELEMENT_MASK) | ELEMENT_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_WIDTH] = {
    .applies_to = BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
              BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_MARKER) |
              BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_PATTERN),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SVG_PROPERTY_HEIGHT] = {
    .applies_to = BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
              BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_MARKER) |
              BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_PATTERN),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SVG_PROPERTY_RX] = {
    .applies_to = BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SVG_PROPERTY_RY] = {
    .applies_to = BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage_or_auto,
    .is_valid = number_is_positive_or_auto,
  },
  [SVG_PROPERTY_X1] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_Y1] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_X2] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_Y2] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_POINTS] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_POLYGON) | BIT (SVG_ELEMENT_POLYLINE),
    .parse_value = parse_points,
  },
  [SVG_PROPERTY_SPREAD_METHOD] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_GRADIENTS,
    .parse_value = svg_spread_method_parse,
  },
  [SVG_PROPERTY_CONTENT_UNITS] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) |
                  BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_coord_units_parse,
  },
  [SVG_PROPERTY_BOUND_UNITS] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_PATTERN) | BIT (SVG_ELEMENT_MASK) | BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_coord_units_parse,
  },
  [SVG_PROPERTY_FX] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_FY] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_FR] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_RADIAL_GRADIENT),
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SVG_PROPERTY_MASK_TYPE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = BIT (SVG_ELEMENT_MASK),
    .parse_value = svg_mask_type_parse,
  },
  [SVG_PROPERTY_VIEW_BOX] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_VIEWPORTS | BIT (SVG_ELEMENT_PATTERN) | BIT (SVG_ELEMENT_VIEW) | BIT (SVG_ELEMENT_MARKER),
    .parse_value = svg_view_box_parse,
  },
  [SVG_PROPERTY_CONTENT_FIT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_VIEWPORTS | BIT (SVG_ELEMENT_PATTERN) | BIT (SVG_ELEMENT_VIEW) | BIT (SVG_ELEMENT_MARKER) | BIT (SVG_ELEMENT_IMAGE),
    .parse_value = svg_content_fit_parse,
  },
  [SVG_PROPERTY_REF_X] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_MARKER),
    .parse_value = parse_ref_x,
  },
  [SVG_PROPERTY_REF_Y] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_MARKER),
    .parse_value = parse_ref_y,
  },
  [SVG_PROPERTY_MARKER_UNITS] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_MARKER),
    .parse_value = svg_marker_units_parse,
  },
  [SVG_PROPERTY_MARKER_ORIENT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_MARKER),
    .parse_value = svg_orient_parse,
  },
  [SVG_PROPERTY_LANG] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_language,
  },
  [SVG_PROPERTY_TEXT_ANCHOR] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_text_anchor_parse,
  },
  [SVG_PROPERTY_DOMINANT_BASELINE] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_dominant_baseline_parse,
  },
  [SVG_PROPERTY_DX] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_DY] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_UNICODE_BIDI] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_unicode_bidi_parse,
  },
  [SVG_PROPERTY_DIRECTION] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_direction_parse,
  },
  [SVG_PROPERTY_WRITING_MODE] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_writing_mode_parse,
  },
  [SVG_PROPERTY_LETTER_SPACING] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = parse_letter_spacing,
  },
  [SVG_PROPERTY_TEXT_DECORATION] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_TEXTS,
    .parse_value = svg_text_decoration_parse,
  },
  [SVG_PROPERTY_REQUIRED_EXTENSIONS] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_ANY,
    .parse_presentation = parse_string_list,
  },
  [SVG_PROPERTY_SYSTEM_LANGUAGE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_ANY,
    .parse_value = parse_language_list,
  },
  [SVG_PROPERTY_POINTER_EVENTS] = {
    .flags = SVG_PROPERTY_IS_DISCRETE,
    .applies_to = ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE),
    .parse_value = svg_pointer_events_parse,
  },
  [SVG_PROPERTY_STROKE_MINWIDTH] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = parse_stroke_width,
  },
  [SVG_PROPERTY_STROKE_MAXWIDTH] = {
    .flags = SVG_PROPERTY_IS_INHERITED | SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_SHAPES,
    .parse_value = parse_stroke_width,
  },
  [SVG_PROPERTY_STOP_OFFSET] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = ELEMENT_GRADIENTS,
    .parse_value = parse_offset,
  },
  [SVG_PROPERTY_STOP_COLOR] = {
    .applies_to = ELEMENT_GRADIENTS,
    .parse_value = svg_color_parse,
  },
  [SVG_PROPERTY_STOP_OPACITY] = {
    .applies_to = ELEMENT_GRADIENTS,
    .parse_value = parse_opacity,
  },
  [SVG_PROPERTY_FE_X] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_FE_Y] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_length_percentage,
  },
  [SVG_PROPERTY_FE_WIDTH] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SVG_PROPERTY_FE_HEIGHT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_length_percentage,
    .is_valid = svg_value_is_positive_number,
  },
  [SVG_PROPERTY_FE_RESULT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_custom_ident,
  },
  [SVG_PROPERTY_FE_COLOR] = {
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_color_parse,
  },
  [SVG_PROPERTY_FE_OPACITY] = {
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_opacity,
  },
  [SVG_PROPERTY_FE_IN] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_filter_ref_parse,
  },
  [SVG_PROPERTY_FE_IN2] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_filter_ref_parse,
  },
  [SVG_PROPERTY_FE_STD_DEV] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_number_optional_number,
  },
  [SVG_PROPERTY_FE_DX] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_length,
  },
  [SVG_PROPERTY_FE_DY] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_length,
  },
  [SVG_PROPERTY_FE_BLUR_EDGE_MODE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_edge_mode_parse,
  },
  [SVG_PROPERTY_FE_BLEND_MODE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_blend_mode_parse,
  },
  [SVG_PROPERTY_FE_BLEND_COMPOSITE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_blend_composite_parse,
  },
  [SVG_PROPERTY_FE_COLOR_MATRIX_TYPE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_color_matrix_type_parse,
  },
  [SVG_PROPERTY_FE_COLOR_MATRIX_VALUES] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_numbers_parse,
  },
  [SVG_PROPERTY_FE_COMPOSITE_OPERATOR] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_composite_operator_parse,
  },
  [SVG_PROPERTY_FE_COMPOSITE_K1] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_COMPOSITE_K2] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_COMPOSITE_K3] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_COMPOSITE_K4] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_DISPLACEMENT_SCALE] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_DISPLACEMENT_X] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_rgba_channel_parse,
  },
  [SVG_PROPERTY_FE_DISPLACEMENT_Y] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_rgba_channel_parse,
  },
  [SVG_PROPERTY_FE_IMAGE_HREF] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_presentation = parse_href,
  },
  [SVG_PROPERTY_FE_IMAGE_CONTENT_FIT] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_content_fit_parse,
  },
  [SVG_PROPERTY_FE_FUNC_TYPE] = {
    .flags = SVG_PROPERTY_IS_DISCRETE | SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_component_transfer_type_parse,
  },
  [SVG_PROPERTY_FE_FUNC_VALUES] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = svg_numbers_parse,
  },
  [SVG_PROPERTY_FE_FUNC_SLOPE] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_FUNC_INTERCEPT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_FUNC_AMPLITUDE] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_FUNC_EXPONENT] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
  [SVG_PROPERTY_FE_FUNC_OFFSET] = {
    .flags = SVG_PROPERTY_NO_CSS,
    .applies_to = BIT (SVG_ELEMENT_FILTER),
    .parse_value = parse_any_number,
  },
};

gboolean
svg_property_inherited (SvgProperty attr)
{
  return (shape_attrs[attr].flags & SVG_PROPERTY_IS_INHERITED) != 0;
}

gboolean
svg_property_discrete (SvgProperty attr)
{
  return (shape_attrs[attr].flags & SVG_PROPERTY_IS_DISCRETE) != 0;
}

gboolean
svg_property_has_presentation (SvgProperty attr)
{
  return (shape_attrs[attr].flags & SVG_PROPERTY_NO_PRESENTATION) == 0;
}

gboolean
svg_property_has_css (SvgProperty attr)
{
  return (shape_attrs[attr].flags & SVG_PROPERTY_NO_CSS) == 0;
}

gboolean
svg_property_applies_to (SvgProperty      attr,
                         SvgElementType type)
{
  return svg_property_inherited (attr) ||
         (shape_attrs[attr].applies_to & BIT (type)) != 0;
}

gboolean
svg_property_for_element (SvgProperty attr)
{
  return attr <= SVG_PROPERTY_STROKE_MAXWIDTH;
}

gboolean
svg_property_for_color_stop (SvgProperty attr)
{
  return SVG_PROPERTY_STOP_OFFSET <= attr && attr <= SVG_PROPERTY_STOP_OPACITY;
}

gboolean
svg_property_for_filter (SvgProperty attr)
{
  return SVG_PROPERTY_FE_X <= attr;
}

static void
shape_attrs_init_default_values (void)
{
  shape_attrs[SVG_PROPERTY_LANG].initial_value = svg_language_new_default ();
  shape_attrs[SVG_PROPERTY_DISPLAY].initial_value = svg_display_new (DISPLAY_INLINE);
  shape_attrs[SVG_PROPERTY_VISIBILITY].initial_value = svg_visibility_new (VISIBILITY_VISIBLE);
  shape_attrs[SVG_PROPERTY_FONT_SIZE].initial_value = svg_font_size_new (FONT_SIZE_MEDIUM);
  shape_attrs[SVG_PROPERTY_TRANSFORM].initial_value = svg_transform_new_none ();
  shape_attrs[SVG_PROPERTY_TRANSFORM_ORIGIN].initial_value = svg_numbers_new_00 ();
 shape_attrs[SVG_PROPERTY_TRANSFORM_BOX].initial_value = svg_transform_box_new (TRANSFORM_BOX_VIEW_BOX);
  shape_attrs[SVG_PROPERTY_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SVG_PROPERTY_COLOR_INTERPOLATION].initial_value = svg_color_interpolation_new (COLOR_INTERPOLATION_SRGB);
  shape_attrs[SVG_PROPERTY_COLOR_INTERPOLATION_FILTERS].initial_value = svg_color_interpolation_new (COLOR_INTERPOLATION_LINEAR);
  shape_attrs[SVG_PROPERTY_OVERFLOW].initial_value = svg_overflow_new (OVERFLOW_VISIBLE);
  shape_attrs[SVG_PROPERTY_VECTOR_EFFECT].initial_value = svg_vector_effect_new (VECTOR_EFFECT_NONE);
  shape_attrs[SVG_PROPERTY_FILTER].initial_value = svg_filter_functions_new_none ();
  shape_attrs[SVG_PROPERTY_CLIP_PATH].initial_value = svg_clip_new_none ();
  shape_attrs[SVG_PROPERTY_CLIP_RULE].initial_value = svg_fill_rule_new (GSK_FILL_RULE_WINDING);
  shape_attrs[SVG_PROPERTY_MASK].initial_value = svg_mask_new_none ();
  shape_attrs[SVG_PROPERTY_FONT_FAMILY].initial_value = svg_string_list_new (NULL);
  shape_attrs[SVG_PROPERTY_FONT_STYLE].initial_value = svg_font_style_new (PANGO_STYLE_NORMAL);
  shape_attrs[SVG_PROPERTY_FONT_VARIANT].initial_value = svg_font_variant_new (PANGO_VARIANT_NORMAL);
  shape_attrs[SVG_PROPERTY_FONT_WEIGHT].initial_value = svg_font_weight_new (FONT_WEIGHT_NORMAL);
  shape_attrs[SVG_PROPERTY_FONT_STRETCH].initial_value = svg_font_stretch_new (PANGO_STRETCH_NORMAL);
  shape_attrs[SVG_PROPERTY_FILL].initial_value = svg_paint_new_black ();
  shape_attrs[SVG_PROPERTY_FILL_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FILL_RULE].initial_value = svg_fill_rule_new (GSK_FILL_RULE_WINDING);
  shape_attrs[SVG_PROPERTY_STROKE].initial_value = svg_paint_new_none ();
  shape_attrs[SVG_PROPERTY_STROKE_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_STROKE_WIDTH].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_STROKE_LINECAP].initial_value = svg_linecap_new (GSK_LINE_CAP_BUTT);
  shape_attrs[SVG_PROPERTY_STROKE_LINEJOIN].initial_value = svg_linejoin_new (GSK_LINE_JOIN_MITER);
  shape_attrs[SVG_PROPERTY_STROKE_MITERLIMIT].initial_value = svg_number_new (4);
  shape_attrs[SVG_PROPERTY_STROKE_DASHARRAY].initial_value = svg_dash_array_new_none ();
  shape_attrs[SVG_PROPERTY_STROKE_DASHOFFSET].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_PAINT_ORDER].initial_value = svg_paint_order_new (PAINT_ORDER_FILL_STROKE_MARKERS);
  shape_attrs[SVG_PROPERTY_SHAPE_RENDERING].initial_value = svg_shape_rendering_new (SHAPE_RENDERING_AUTO);
  shape_attrs[SVG_PROPERTY_TEXT_RENDERING].initial_value = svg_text_rendering_new (TEXT_RENDERING_AUTO);
  shape_attrs[SVG_PROPERTY_IMAGE_RENDERING].initial_value = svg_image_rendering_new (IMAGE_RENDERING_AUTO);
  shape_attrs[SVG_PROPERTY_BLEND_MODE].initial_value = svg_blend_mode_new (GSK_BLEND_MODE_DEFAULT);
  shape_attrs[SVG_PROPERTY_ISOLATION].initial_value = svg_isolation_new (ISOLATION_AUTO);
  shape_attrs[SVG_PROPERTY_HREF].initial_value = svg_href_new_none ();
  shape_attrs[SVG_PROPERTY_PATH_LENGTH].initial_value = svg_number_new (-1);
  shape_attrs[SVG_PROPERTY_PATH].initial_value = svg_path_new_none ();
  shape_attrs[SVG_PROPERTY_CX].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_CY].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_R].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_X].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_Y].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_WIDTH].initial_value = svg_auto_new ();
  shape_attrs[SVG_PROPERTY_HEIGHT].initial_value = svg_auto_new ();
  shape_attrs[SVG_PROPERTY_RX].initial_value = svg_auto_new ();
  shape_attrs[SVG_PROPERTY_RY].initial_value = svg_auto_new ();
  shape_attrs[SVG_PROPERTY_X1].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_Y1].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_X2].initial_value = svg_percentage_new (100);
  shape_attrs[SVG_PROPERTY_Y2].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_FX].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FY].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FR].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_TEXT_ANCHOR].initial_value = svg_text_anchor_new (TEXT_ANCHOR_START);
  shape_attrs[SVG_PROPERTY_DOMINANT_BASELINE].initial_value = svg_dominant_baseline_new (DOMINANT_BASELINE_AUTO);
  shape_attrs[SVG_PROPERTY_DX].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_DY].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_UNICODE_BIDI].initial_value = svg_unicode_bidi_new (UNICODE_BIDI_NORMAL);
  shape_attrs[SVG_PROPERTY_DIRECTION].initial_value = svg_direction_new (PANGO_DIRECTION_LTR);
  shape_attrs[SVG_PROPERTY_WRITING_MODE].initial_value = svg_writing_mode_new (WRITING_MODE_HORIZONTAL_TB);
  shape_attrs[SVG_PROPERTY_LETTER_SPACING].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_TEXT_DECORATION].initial_value = svg_text_decoration_new (TEXT_DECORATION_NONE);
  shape_attrs[SVG_PROPERTY_POINTS].initial_value = svg_numbers_new_none ();
  shape_attrs[SVG_PROPERTY_SPREAD_METHOD].initial_value = svg_spread_method_new (GSK_REPEAT_PAD);
  shape_attrs[SVG_PROPERTY_CONTENT_UNITS].initial_value = svg_coord_units_new (COORD_UNITS_OBJECT_BOUNDING_BOX);
  shape_attrs[SVG_PROPERTY_BOUND_UNITS].initial_value = svg_coord_units_new (COORD_UNITS_OBJECT_BOUNDING_BOX);
  shape_attrs[SVG_PROPERTY_MASK_TYPE].initial_value = svg_mask_type_new (GSK_MASK_MODE_LUMINANCE);
  shape_attrs[SVG_PROPERTY_VIEW_BOX].initial_value = svg_view_box_new_unset ();
  shape_attrs[SVG_PROPERTY_CONTENT_FIT].initial_value = svg_content_fit_new (ALIGN_MID, ALIGN_MID, MEET);
  shape_attrs[SVG_PROPERTY_REF_X].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_REF_Y].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_MARKER_UNITS].initial_value = svg_marker_units_new (MARKER_UNITS_STROKE_WIDTH);
  shape_attrs[SVG_PROPERTY_MARKER_ORIENT].initial_value = svg_orient_new_angle (0, SVG_UNIT_NUMBER);
  shape_attrs[SVG_PROPERTY_MARKER_START].initial_value = svg_href_new_none ();
  shape_attrs[SVG_PROPERTY_MARKER_MID].initial_value = svg_href_new_none ();
  shape_attrs[SVG_PROPERTY_MARKER_END].initial_value = svg_href_new_none ();
  shape_attrs[SVG_PROPERTY_REQUIRED_EXTENSIONS].initial_value = svg_string_list_new (NULL);
  shape_attrs[SVG_PROPERTY_SYSTEM_LANGUAGE].initial_value = svg_language_new_list (0, NULL);
  shape_attrs[SVG_PROPERTY_POINTER_EVENTS].initial_value = svg_pointer_events_new (POINTER_EVENTS_AUTO);
  shape_attrs[SVG_PROPERTY_STROKE_MINWIDTH].initial_value = svg_percentage_new (25);
  shape_attrs[SVG_PROPERTY_STROKE_MAXWIDTH].initial_value = svg_percentage_new (150);
  shape_attrs[SVG_PROPERTY_STOP_OFFSET].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_STOP_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SVG_PROPERTY_STOP_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_X].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_FE_Y].initial_value = svg_percentage_new (0);
  shape_attrs[SVG_PROPERTY_FE_WIDTH].initial_value = svg_percentage_new (100);
  shape_attrs[SVG_PROPERTY_FE_HEIGHT].initial_value = svg_percentage_new (100);
  shape_attrs[SVG_PROPERTY_FE_RESULT].initial_value = svg_string_new ("");
  shape_attrs[SVG_PROPERTY_FE_COLOR].initial_value = svg_color_new_black ();
  shape_attrs[SVG_PROPERTY_FE_OPACITY].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_IN].initial_value = svg_filter_ref_new (FILTER_REF_DEFAULT_SOURCE);
  shape_attrs[SVG_PROPERTY_FE_IN2].initial_value = svg_filter_ref_new (FILTER_REF_DEFAULT_SOURCE);
  shape_attrs[SVG_PROPERTY_FE_STD_DEV].initial_value = svg_numbers_new1 (0);
  shape_attrs[SVG_PROPERTY_FE_BLUR_EDGE_MODE].initial_value = svg_edge_mode_new (EDGE_MODE_NONE);
  shape_attrs[SVG_PROPERTY_FE_BLEND_MODE].initial_value = svg_blend_mode_new (GSK_BLEND_MODE_DEFAULT);
  shape_attrs[SVG_PROPERTY_FE_BLEND_COMPOSITE].initial_value = svg_blend_composite_new (BLEND_COMPOSITE);
  shape_attrs[SVG_PROPERTY_FE_COLOR_MATRIX_TYPE].initial_value = svg_color_matrix_type_new (COLOR_MATRIX_TYPE_MATRIX);
  shape_attrs[SVG_PROPERTY_FE_COLOR_MATRIX_VALUES].initial_value = svg_numbers_new_identity_matrix ();
  shape_attrs[SVG_PROPERTY_FE_COMPOSITE_OPERATOR].initial_value = svg_composite_operator_new (COMPOSITE_OPERATOR_OVER);
  shape_attrs[SVG_PROPERTY_FE_COMPOSITE_K1].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_COMPOSITE_K2].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_COMPOSITE_K3].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_COMPOSITE_K4].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_DX].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_DY].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_DISPLACEMENT_SCALE].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_DISPLACEMENT_X].initial_value = svg_rgba_channel_new (GDK_COLOR_CHANNEL_ALPHA);
  shape_attrs[SVG_PROPERTY_FE_DISPLACEMENT_Y].initial_value = svg_rgba_channel_new (GDK_COLOR_CHANNEL_ALPHA);
  shape_attrs[SVG_PROPERTY_FE_IMAGE_HREF].initial_value = svg_href_new_none ();
  shape_attrs[SVG_PROPERTY_FE_IMAGE_CONTENT_FIT].initial_value = svg_content_fit_new (ALIGN_MID, ALIGN_MID, MEET);
  shape_attrs[SVG_PROPERTY_FE_FUNC_TYPE].initial_value = svg_component_transfer_type_new (COMPONENT_TRANSFER_IDENTITY);
  shape_attrs[SVG_PROPERTY_FE_FUNC_VALUES].initial_value = svg_numbers_new_none ();
  shape_attrs[SVG_PROPERTY_FE_FUNC_SLOPE].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_FUNC_INTERCEPT].initial_value = svg_number_new (0);
  shape_attrs[SVG_PROPERTY_FE_FUNC_AMPLITUDE].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_FUNC_EXPONENT].initial_value = svg_number_new (1);
  shape_attrs[SVG_PROPERTY_FE_FUNC_OFFSET].initial_value = svg_number_new (0);

  /* We require initial values to immortal for thread-safety
   * reasons. since they are the only objects in the SVG code
   * that are shared between different GtkSvg instances (and
   * thus may be shared between threads), and our refcounting
   * for values is not atomic.
   */
#ifndef G_DISABLE_ASSERT
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attrs); i++)
    {
      g_assert (svg_value_is_immortal (shape_attrs[i].initial_value));
      g_assert (shape_attrs[i].parse_value != NULL ||
                ((shape_attrs[i].flags & SVG_PROPERTY_NO_CSS) != 0 &&
                 shape_attrs[i].parse_presentation != NULL));
    }
#endif
}

/* Sadly, initial values for properties of SVG objects
 * are confusingly dependent on the type of the object.
 */
SvgValue *
svg_property_ref_initial_value (SvgProperty    attr,
                                SvgElementType shape_type,
                                gboolean       has_parent)
{
  if (shape_type == SVG_ELEMENT_RADIAL_GRADIENT &&
      (attr == SVG_PROPERTY_CX || attr == SVG_PROPERTY_CY || attr == SVG_PROPERTY_R))
    return svg_percentage_new (50);

  if (shape_type == SVG_ELEMENT_LINE &&
      (attr == SVG_PROPERTY_X1 || attr == SVG_PROPERTY_Y1 ||
       attr == SVG_PROPERTY_X2 || attr == SVG_PROPERTY_Y2))
    return svg_number_new (0);

  if ((shape_type == SVG_ELEMENT_CLIP_PATH || shape_type == SVG_ELEMENT_MASK ||
       shape_type == SVG_ELEMENT_PATTERN || shape_type == SVG_ELEMENT_FILTER) &&
      attr == SVG_PROPERTY_CONTENT_UNITS)
    return svg_coord_units_new (COORD_UNITS_USER_SPACE_ON_USE);

  if (shape_type == SVG_ELEMENT_MASK || shape_type == SVG_ELEMENT_FILTER)
    {
      if (attr == SVG_PROPERTY_X || attr == SVG_PROPERTY_Y)
        return svg_percentage_new (-10);
      if (attr == SVG_PROPERTY_WIDTH || attr == SVG_PROPERTY_HEIGHT)
        return svg_percentage_new (120);
    }

  if (shape_type == SVG_ELEMENT_MARKER || shape_type == SVG_ELEMENT_PATTERN ||
      shape_type == SVG_ELEMENT_IMAGE)
    {
      if (attr == SVG_PROPERTY_OVERFLOW)
        return svg_overflow_new (OVERFLOW_HIDDEN);
    }

  if (shape_type == SVG_ELEMENT_MARKER)
    {
      if (attr == SVG_PROPERTY_WIDTH || attr == SVG_PROPERTY_HEIGHT)
        return svg_number_new (3);
    }

  if (shape_type == SVG_ELEMENT_SVG || shape_type == SVG_ELEMENT_SYMBOL)
    {
      if (attr == SVG_PROPERTY_WIDTH || attr == SVG_PROPERTY_HEIGHT)
        return svg_percentage_new (100);
      if (attr == SVG_PROPERTY_OVERFLOW && has_parent)
        return svg_overflow_new (OVERFLOW_HIDDEN);
    }

  if ((shape_type == SVG_ELEMENT_CLIP_PATH || shape_type == SVG_ELEMENT_MASK ||
       shape_type == SVG_ELEMENT_DEFS || shape_type == SVG_ELEMENT_MARKER ||
       shape_type == SVG_ELEMENT_PATTERN || shape_type == SVG_ELEMENT_LINEAR_GRADIENT ||
       shape_type == SVG_ELEMENT_RADIAL_GRADIENT) &&
      attr == SVG_PROPERTY_DISPLAY)
    return svg_display_new (DISPLAY_NONE);

  return svg_value_ref (shape_attrs[attr].initial_value);
}

typedef struct {
  const char *name;
  unsigned int shapes;
  unsigned int filters;
  SvgProperty attr;
} SvgPropertyLookup;

#define FILTER_ANY \
  (BIT (SVG_FILTER_FLOOD) | BIT (SVG_FILTER_BLUR) | BIT (SVG_FILTER_BLEND) | \
   BIT (SVG_FILTER_COLOR_MATRIX) | BIT (SVG_FILTER_COMPOSITE) | BIT (SVG_FILTER_OFFSET) | \
   BIT (SVG_FILTER_DISPLACEMENT) | BIT (SVG_FILTER_TILE) | BIT (SVG_FILTER_IMAGE) | \
   BIT (SVG_FILTER_MERGE) | BIT (SVG_FILTER_COMPONENT_TRANSFER) | BIT (SVG_FILTER_DROPSHADOW))

#define FILTER_FUNCS \
  (BIT (SVG_FILTER_FUNC_R) | BIT (SVG_FILTER_FUNC_G) | BIT (SVG_FILTER_FUNC_B) | BIT (SVG_FILTER_FUNC_A))

static SvgPropertyLookup shape_attr_lookups[] = {
  { "display", ELEMENT_ANY, 0, SVG_PROPERTY_DISPLAY },
  { "visibility", ELEMENT_ANY, 0, SVG_PROPERTY_VISIBILITY },
  { "font-size", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_SIZE },
  { "transform", ((ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN)) & ~ELEMENT_PAINT_SERVERS, 0, SVG_PROPERTY_TRANSFORM },
  { "gradientTransform", ELEMENT_GRADIENTS, 0, SVG_PROPERTY_TRANSFORM },
  { "patternTransform", BIT (SVG_ELEMENT_PATTERN), 0, SVG_PROPERTY_TRANSFORM },
  { "transform-origin", ((ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN)), 0, SVG_PROPERTY_TRANSFORM_ORIGIN },
  { "transform-box", ((ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_CLIP_PATH) | ELEMENT_RENDERABLE) & ~BIT (SVG_ELEMENT_TSPAN)), 0, SVG_PROPERTY_TRANSFORM_BOX },
  { "opacity", ELEMENT_ANY, 0, SVG_PROPERTY_OPACITY },
  { "color", ELEMENT_ANY, 0, SVG_PROPERTY_COLOR },
  { "color-interpolation", ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRADIENTS | SVG_ELEMENT_USE, 0, SVG_PROPERTY_COLOR_INTERPOLATION },
  { "color-interpolation-filters", BIT (SVG_ELEMENT_FILTER), 0, SVG_PROPERTY_COLOR_INTERPOLATION_FILTERS },
  { "clip-path", (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE), 0, SVG_PROPERTY_CLIP_PATH },
  { "clip-rule", ELEMENT_ANY, 0, SVG_PROPERTY_CLIP_RULE },
  { "mask", (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE), 0, SVG_PROPERTY_MASK },
  { "font-family", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_FAMILY },
  { "font-style", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_STYLE },
  { "font-variant", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_VARIANT },
  { "font-weight", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_WEIGHT },
  { "font-stretch", ELEMENT_ANY, 0, SVG_PROPERTY_FONT_STRETCH },
  { "filter", (ELEMENT_CONTAINERS & ~BIT (SVG_ELEMENT_DEFS)) | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE), 0, SVG_PROPERTY_FILTER },
  { "fill", ELEMENT_ANY, 0, SVG_PROPERTY_FILL },
  { "fill-opacity", ELEMENT_ANY, 0, SVG_PROPERTY_FILL_OPACITY },
  { "fill-rule", ELEMENT_ANY, 0, SVG_PROPERTY_FILL_RULE },
  { "stroke", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE },
  { "stroke-opacity", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_OPACITY },
  { "stroke-width", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_WIDTH },
  { "stroke-linecap", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_LINECAP },
  { "stroke-linejoin", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_LINEJOIN },
  { "stroke-miterlimit", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_MITERLIMIT },
  { "stroke-dasharray", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_DASHARRAY },
  { "stroke-dashoffset", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_DASHOFFSET },
  { "marker-start", ELEMENT_ANY, 0, SVG_PROPERTY_MARKER_START },
  { "marker-mid", ELEMENT_ANY, 0, SVG_PROPERTY_MARKER_MID },
  { "marker-end", ELEMENT_ANY, 0, SVG_PROPERTY_MARKER_END },
  { "paint-order", ELEMENT_ANY, 0, SVG_PROPERTY_PAINT_ORDER },
  { "shape-rendering", ELEMENT_ANY, 0, SVG_PROPERTY_SHAPE_RENDERING },
  { "text-rendering", ELEMENT_ANY, 0, SVG_PROPERTY_TEXT_RENDERING },
  { "image-rendering", ELEMENT_ANY, 0, SVG_PROPERTY_IMAGE_RENDERING },
  { "mix-blend-mode", ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRAPHICS_REF, 0, SVG_PROPERTY_BLEND_MODE },
  { "isolation", ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | ELEMENT_GRAPHICS_REF, 0, SVG_PROPERTY_ISOLATION },
  { "pathLength", ELEMENT_SHAPES, 0, SVG_PROPERTY_PATH_LENGTH },
  { "href", ELEMENT_GRAPHICS_REF | ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_LINK), 0, SVG_PROPERTY_HREF },
  { "xlink:href", ELEMENT_GRAPHICS_REF | ELEMENT_PAINT_SERVERS | BIT (SVG_ELEMENT_LINK), 0, SVG_PROPERTY_HREF },
  { "overflow", ELEMENT_ANY, 0, SVG_PROPERTY_OVERFLOW },
  { "vector-effect", ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE), 0, SVG_PROPERTY_VECTOR_EFFECT },
  { "d", BIT (SVG_ELEMENT_PATH), 0, SVG_PROPERTY_PATH },
  { "cx", BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_CX },
  { "cy", BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_CY },
  { "r", BIT (SVG_ELEMENT_CIRCLE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_R },
  { "x",  BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
          BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_PATTERN) |
          BIT (SVG_ELEMENT_MASK) | ELEMENT_TEXTS, 0, SVG_PROPERTY_X },
  { "y",  BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
          BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_PATTERN) |
          BIT (SVG_ELEMENT_MASK) | ELEMENT_TEXTS, 0, SVG_PROPERTY_Y },
  { "width", BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
             BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_MASK) |
             BIT (SVG_ELEMENT_PATTERN), 0, SVG_PROPERTY_WIDTH },
  { "height", BIT (SVG_ELEMENT_SVG) | BIT (SVG_ELEMENT_SYMBOL) | BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_IMAGE) |
              BIT (SVG_ELEMENT_USE) | BIT (SVG_ELEMENT_FILTER) | BIT (SVG_ELEMENT_MASK) |
              BIT (SVG_ELEMENT_PATTERN), 0, SVG_PROPERTY_HEIGHT },
  { "markerWidth", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_WIDTH },
  { "markerHeight", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_HEIGHT },
  { "rx", BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_RX },
  { "ry", BIT (SVG_ELEMENT_RECT) | BIT (SVG_ELEMENT_ELLIPSE) | BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_RY },
  { "x1", BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT), 0, SVG_PROPERTY_X1 },
  { "y1", BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT), 0, SVG_PROPERTY_Y1 },
  { "x2", BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT), 0, SVG_PROPERTY_X2 },
  { "y2", BIT (SVG_ELEMENT_LINE) | BIT (SVG_ELEMENT_LINEAR_GRADIENT), 0, SVG_PROPERTY_Y2 },
  { "points", BIT (SVG_ELEMENT_POLYGON) | BIT (SVG_ELEMENT_POLYLINE), 0, SVG_PROPERTY_POINTS },
  { "spreadMethod", ELEMENT_GRADIENTS, 0, SVG_PROPERTY_SPREAD_METHOD },
  { "gradientUnits", ELEMENT_GRADIENTS, 0, SVG_PROPERTY_CONTENT_UNITS },
  { "clipPathUnits", BIT (SVG_ELEMENT_CLIP_PATH), 0, SVG_PROPERTY_CONTENT_UNITS },
  { "maskContentUnits", BIT (SVG_ELEMENT_MASK), 0, SVG_PROPERTY_CONTENT_UNITS },
  { "patternContentUnits", BIT (SVG_ELEMENT_PATTERN), 0, SVG_PROPERTY_CONTENT_UNITS },
  { "primitiveUnits", BIT (SVG_ELEMENT_FILTER), 0, SVG_PROPERTY_CONTENT_UNITS },
  { "maskUnits", BIT (SVG_ELEMENT_MASK), 0, SVG_PROPERTY_BOUND_UNITS },
  { "patternUnits", BIT (SVG_ELEMENT_PATTERN), 0, SVG_PROPERTY_BOUND_UNITS },
  { "filterUnits", BIT (SVG_ELEMENT_FILTER), 0, SVG_PROPERTY_BOUND_UNITS },
  { "fx", BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_FX },
  { "fy", BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_FY },
  { "fr", BIT (SVG_ELEMENT_RADIAL_GRADIENT), 0, SVG_PROPERTY_FR },
  { "mask-type", BIT (SVG_ELEMENT_MASK), 0, SVG_PROPERTY_MASK_TYPE },
  { "viewBox", ELEMENT_VIEWPORTS | BIT (SVG_ELEMENT_PATTERN) | BIT (SVG_ELEMENT_VIEW) | BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_VIEW_BOX },
  { "preserveAspectRatio", ELEMENT_VIEWPORTS | BIT (SVG_ELEMENT_PATTERN) | BIT (SVG_ELEMENT_VIEW) | BIT (SVG_ELEMENT_MARKER) | BIT (SVG_ELEMENT_IMAGE), 0, SVG_PROPERTY_CONTENT_FIT },
  { "refX", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_REF_X },
  { "refY", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_REF_Y },
  { "markerUnits", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_MARKER_UNITS },
  { "orient", BIT (SVG_ELEMENT_MARKER), 0, SVG_PROPERTY_MARKER_ORIENT },
  { "lang", ELEMENT_ANY, 0, SVG_PROPERTY_LANG },
  { "xml:lang", ELEMENT_ANY, 0, SVG_PROPERTY_LANG },
  { "text-anchor", ELEMENT_ANY, 0, SVG_PROPERTY_TEXT_ANCHOR },
  { "dominant-baseline", ELEMENT_ANY, 0, SVG_PROPERTY_DOMINANT_BASELINE },
  { "dx", ELEMENT_TEXTS, 0, SVG_PROPERTY_DX },
  { "dy", ELEMENT_TEXTS, 0, SVG_PROPERTY_DY },
  { "unicode-bidi", ELEMENT_ANY, 0, SVG_PROPERTY_UNICODE_BIDI },
  { "direction", ELEMENT_ANY, 0, SVG_PROPERTY_DIRECTION },
  { "writing-mode", ELEMENT_ANY, 0, SVG_PROPERTY_WRITING_MODE },
  { "letter-spacing", ELEMENT_ANY, 0, SVG_PROPERTY_LETTER_SPACING },
  { "text-decoration", ELEMENT_ANY, 0, SVG_PROPERTY_TEXT_DECORATION },
  { "requiredExtensions", ELEMENT_ANY, 0, SVG_PROPERTY_REQUIRED_EXTENSIONS },
  { "systemLanguage", ELEMENT_ANY, 0, SVG_PROPERTY_SYSTEM_LANGUAGE },
  { "pointer-events", ELEMENT_CONTAINERS | ELEMENT_GRAPHICS | BIT (SVG_ELEMENT_USE), 0, SVG_PROPERTY_POINTER_EVENTS },
  { "gpa:stroke-minwidth", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_MINWIDTH },
  { "gpa:stroke-maxwidth", ELEMENT_ANY, 0, SVG_PROPERTY_STROKE_MAXWIDTH },
  { "offset", ELEMENT_GRADIENTS, 0, SVG_PROPERTY_STOP_OFFSET },
  { "stop-color", ELEMENT_ANY, 0, SVG_PROPERTY_STOP_COLOR },
  { "stop-opacity", ELEMENT_ANY, 0, SVG_PROPERTY_STOP_OPACITY },
  { "x", BIT (SVG_ELEMENT_FILTER), FILTER_ANY, SVG_PROPERTY_FE_X },
  { "y", BIT (SVG_ELEMENT_FILTER), FILTER_ANY, SVG_PROPERTY_FE_Y },
  { "width", BIT (SVG_ELEMENT_FILTER), FILTER_ANY, SVG_PROPERTY_FE_WIDTH },
  { "height", BIT (SVG_ELEMENT_FILTER), FILTER_ANY, SVG_PROPERTY_FE_HEIGHT },
  { "result", BIT (SVG_ELEMENT_FILTER), FILTER_ANY, SVG_PROPERTY_FE_RESULT },
  { "flood-color", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_FLOOD) | BIT (SVG_FILTER_DROPSHADOW), SVG_PROPERTY_FE_COLOR },
  { "flood-color", ELEMENT_ANY, 0, SVG_PROPERTY_FE_COLOR },
  { "flood-opacity", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_FLOOD) | BIT (SVG_FILTER_DROPSHADOW), SVG_PROPERTY_FE_OPACITY },
  { "flood-opacity", ELEMENT_ANY, 0, SVG_PROPERTY_FE_OPACITY },
  { "in", BIT (SVG_ELEMENT_FILTER), (FILTER_ANY | BIT (SVG_FILTER_MERGE_NODE)) & ~(BIT (SVG_FILTER_FLOOD) | BIT (SVG_FILTER_IMAGE) | BIT (SVG_FILTER_MERGE)), SVG_PROPERTY_FE_IN },
  { "in2", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_BLEND) | BIT (SVG_FILTER_COMPOSITE) | BIT (SVG_FILTER_DISPLACEMENT), SVG_PROPERTY_FE_IN2 },
  { "stdDeviation", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_BLUR) | BIT (SVG_FILTER_DROPSHADOW), SVG_PROPERTY_FE_STD_DEV },
  { "dx", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_OFFSET) | BIT (SVG_FILTER_DROPSHADOW), SVG_PROPERTY_FE_DX },
  { "dy", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_OFFSET) | BIT (SVG_FILTER_DROPSHADOW), SVG_PROPERTY_FE_DY },
  { "edgeMode", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_BLUR), SVG_PROPERTY_FE_BLUR_EDGE_MODE },
  { "mode", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_BLEND), SVG_PROPERTY_FE_BLEND_MODE },
  { "no-composite", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_BLEND), SVG_PROPERTY_FE_BLEND_COMPOSITE },
  { "type", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COLOR_MATRIX), SVG_PROPERTY_FE_COLOR_MATRIX_TYPE },
  { "values", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COLOR_MATRIX), SVG_PROPERTY_FE_COLOR_MATRIX_VALUES },
  { "operator", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COMPOSITE), SVG_PROPERTY_FE_COMPOSITE_OPERATOR },
  { "k1", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COMPOSITE), SVG_PROPERTY_FE_COMPOSITE_K1 },
  { "k2", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COMPOSITE), SVG_PROPERTY_FE_COMPOSITE_K2 },
  { "k3", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COMPOSITE), SVG_PROPERTY_FE_COMPOSITE_K3 },
  { "k4", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_COMPOSITE), SVG_PROPERTY_FE_COMPOSITE_K4 },
  { "scale", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_DISPLACEMENT), SVG_PROPERTY_FE_DISPLACEMENT_SCALE },
  { "xChannelSelector", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_DISPLACEMENT), SVG_PROPERTY_FE_DISPLACEMENT_X },
  { "yChannelSelector", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_DISPLACEMENT), SVG_PROPERTY_FE_DISPLACEMENT_Y },
  { "href", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_IMAGE), SVG_PROPERTY_FE_IMAGE_HREF },
  { "xlink:href", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_IMAGE), SVG_PROPERTY_FE_IMAGE_HREF },
  { "preserveAspectRatio", BIT (SVG_ELEMENT_FILTER), BIT (SVG_FILTER_IMAGE), SVG_PROPERTY_FE_IMAGE_CONTENT_FIT },
  { "type", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_TYPE },
  { "tableValues", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_VALUES },
  { "slope", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_SLOPE },
  { "intercept", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_INTERCEPT },
  { "amplitude", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_AMPLITUDE },
  { "exponent", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_EXPONENT },
  { "offset", BIT (SVG_ELEMENT_FILTER), FILTER_FUNCS, SVG_PROPERTY_FE_FUNC_OFFSET },
};

gboolean
svg_attr_is_deprecated (const char *name)
{
  return strcmp (name, "xlink:href") == 0 || strcmp (name, "xml:lang") == 0;
}

static unsigned int
shape_attr_lookup_hash (gconstpointer v)
{
  const SvgPropertyLookup *l = (const SvgPropertyLookup *) v;

  return g_str_hash (l->name);
}

/* This is a slightly tricky use of a hash table, since this relationship
 * is not transitive, in general. It is for our use case, by the way the
 * table is constructed.
 *
 * Names can occur multiple times in the table, but each (name, shape)
 * pair will only occur once. Attributes can also occur multiple times
 * (e.g. transform vs gradientTransform vs patternTransform).
 *
 * We use the array too, to find names for attributes.
 */
static gboolean
shape_attr_lookup_equal (gconstpointer v0,
                         gconstpointer v1)
{
  const SvgPropertyLookup *l0 = (const SvgPropertyLookup *) v0;
  const SvgPropertyLookup *l1 = (const SvgPropertyLookup *) v1;

  return strcmp (l0->name, l1->name) == 0 &&
         (l0->shapes & l1->shapes) != 0 &&
         ((l0->filters == 0 && l1->filters == 0) ||
          (l0->filters & l1->filters) != 0);
}

static GHashTable *shape_attr_lookup_table;

static void
shape_attrs_init_lookup (void)
{
  shape_attr_lookup_table = g_hash_table_new (shape_attr_lookup_hash,
                                              shape_attr_lookup_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    g_hash_table_add (shape_attr_lookup_table, &shape_attr_lookups[i]);
}

gboolean
svg_property_lookup (const char     *name,
                     SvgElementType  type,
                     SvgProperty    *attr)
{
  SvgPropertyLookup key;
  SvgPropertyLookup *found;

  key.name = name;
  key.shapes = BIT (type);
  key.filters = 0;

  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    return FALSE;

  g_assert ((found->shapes & BIT (type)) != 0);

  *attr = found->attr;
  return TRUE;
}

gboolean
svg_property_lookup_for_css (const char  *name,
                             SvgProperty *result)
{
  SvgPropertyLookup key;
  SvgPropertyLookup *found;

  key.name = name;
  key.shapes = ELEMENT_ANY;
  key.filters = 0;
  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    {
      key.shapes = SVG_ELEMENT_FILTER;
      key.filters = FILTER_ANY;
      found = g_hash_table_lookup (shape_attr_lookup_table, &key);
    }

  if (!found)
    return FALSE;

  *result = found->attr;
  return svg_property_has_css (*result);
}

gboolean
svg_property_lookup_for_stop (const char     *name,
                              SvgElementType  type,
                              SvgProperty    *result)
{
  SvgPropertyLookup key;
  SvgPropertyLookup *found;

  key.name = name;
  key.shapes = BIT (type);
  key.filters = 0;

  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    return FALSE;

  *result = found->attr;

  if (*result < FIRST_STOP_PROPERTY || *result > LAST_STOP_PROPERTY)
    return FALSE;

  return TRUE;
}

gboolean
svg_property_lookup_for_filter (const char     *name,
                                SvgElementType  type,
                                SvgFilterType   filter,
                                SvgProperty    *result)
{
  SvgPropertyLookup key;
  SvgPropertyLookup *found;

  key.name = name;
  key.shapes = BIT (type);
  key.filters = BIT (filter);

  found = g_hash_table_lookup (shape_attr_lookup_table, &key);

  if (!found)
    return FALSE;

  g_assert ((found->shapes & BIT (type)) != 0);
  g_assert ((found->filters & BIT (filter)) != 0);

  *result = found->attr;
  return TRUE;
}

const char *
svg_property_get_name (SvgProperty attr)
{
  for (unsigned int i = attr; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    {
      SvgPropertyLookup *l = &shape_attr_lookups[i];

      if (l->attr == attr)
        return l->name;
    }

  return NULL;
}

const char *
svg_property_get_presentation (SvgProperty    attr,
                               SvgElementType type)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (shape_attr_lookups); i++)
    {
      SvgPropertyLookup *l = &shape_attr_lookups[i];

      if (l->attr == attr && (l->shapes & BIT (type)) != 0)
        return l->name;
    }

  return NULL;
}

static void
parse_value_error (GtkCssParser         *parser,
                   const GtkCssLocation *css_start,
                   const GtkCssLocation *css_end,
                   const GError         *css_error,
                   gpointer              user_data)
{
  /* FIXME: locations */
  if (user_data && !*(GError **) user_data)
    *((GError **) user_data) = g_error_copy (css_error);
}

SvgValue *
svg_property_parse (SvgProperty   attr,
                    const char   *string,
                    GError      **error)
{
  GBytes *bytes;
  GtkCssParser *parser;
  SvgValue *value = NULL;

  bytes = g_bytes_new_static (string, strlen (string));
  parser = gtk_css_parser_new_for_bytes (bytes, NULL, parse_value_error, error, NULL);
  g_bytes_unref (bytes);

  gtk_css_parser_skip_whitespace (parser);
  if (gtk_css_parser_try_ident (parser, "inherit"))
    value = svg_inherit_new ();
  else if (gtk_css_parser_try_ident (parser, "initial"))
    value = svg_initial_new ();

  if (!value)
    {
      if (shape_attrs[attr].parse_presentation)
        {
          value = shape_attrs[attr].parse_presentation (string, error);
          gtk_css_parser_unref (parser);
          return value;
        }
      else
        value = shape_attrs[attr].parse_value (parser);
    }

  gtk_css_parser_skip_whitespace (parser);
  if (value && !gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      g_clear_pointer (&value, svg_value_unref);
      g_clear_error (error);
      g_set_error (error, GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_SYNTAX, "Junk at end of value");
    }

  gtk_css_parser_unref (parser);
  return value;
}

static gboolean
shape_attr_value_is_valid (SvgProperty  attr,
                           SvgValue    *value)
{
  if (shape_attrs[attr].is_valid)
    return shape_attrs[attr].is_valid (value);
  return TRUE;
}

SvgValue *
svg_property_parse_and_validate (SvgProperty   attr,
                                 const char   *string,
                                 GError      **error)
{
  SvgValue *value;

  value = svg_property_parse (attr, string, error);
  if (value && !shape_attr_value_is_valid (attr, value))
    {
      g_clear_pointer (&value, svg_value_unref);
      g_clear_error (error);
      g_set_error (error,
                   GTK_SVG_ERROR, GTK_SVG_ERROR_INVALID_ATTRIBUTE,
                   "Value is not valid");
    }

  return value;
}

SvgValue *
svg_property_parse_css (SvgProperty   attr,
                        GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "inherit"))
    return svg_inherit_new ();
  else if (gtk_css_parser_try_ident (parser, "initial"))
    return svg_initial_new ();
  else
    {
      SvgValue *value;

      value = shape_attrs[attr].parse_value (parser);
      if (value && !shape_attr_value_is_valid (attr, value))
        g_clear_pointer (&value, svg_value_unref);

      return value;
    }
}

GPtrArray *
svg_property_parse_values (SvgProperty    attr,
                           TransformType  transform_type,
                           const char    *value)
{
  GPtrArray *array;
  GStrv strv;

  strv = g_strsplit (value, ";", 0);

  array = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_value_unref);

  for (unsigned int i = 0; strv[i]; i++)
    {
      char *s = g_strstrip (strv[i]);
      SvgValue *v;

      if (*s == '\0' && strv[i + 1] == NULL)
        break;

      if (attr == SVG_PROPERTY_TRANSFORM && transform_type != TRANSFORM_NONE)
        v = primitive_transform_parse (transform_type, s);
      else
        v = svg_property_parse (attr, s, NULL);

      if (!v)
        {
          g_ptr_array_unref (array);
          g_strfreev (strv);
          return NULL;
        }

      g_ptr_array_add (array, v);
    }

  g_strfreev (strv);
  return array;
}

void
svg_properties_init (void)
{
  shape_attrs_init_default_values ();
  shape_attrs_init_lookup ();
}
