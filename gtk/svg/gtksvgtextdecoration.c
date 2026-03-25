#include "gtksvgtextdecorationprivate.h"
#include "gtksvgvalueprivate.h"

static const struct {
  const char *name;
  TextDecoration value;
} text_decorations[] = {
  { .name = "underline", .value = TEXT_DECORATION_UNDERLINE },
  { .name = "overline", .value = TEXT_DECORATION_OVERLINE },
  { .name = "line-through", .value = TEXT_DECORATION_LINE_TROUGH },
};

typedef struct
{
  SvgValue base;
  TextDecoration value;
} SvgTextDecoration;

static gboolean
svg_text_decoration_equal (const SvgValue *value0,
                           const SvgValue *value1)
{
  const SvgTextDecoration *d0 = (const SvgTextDecoration *)value0;
  const SvgTextDecoration *d1 = (const SvgTextDecoration *)value1;

  return d0->value == d1->value;
}

static SvgValue *
svg_text_decoration_interpolate (const SvgValue    *value0,
                                 const SvgValue    *value1,
                                 SvgComputeContext *context,
                                 double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_text_decoration_accumulate (const SvgValue    *value0,
                                const SvgValue    *value1,
                                SvgComputeContext *context,
                                int                n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_text_decoration_print (const SvgValue *value,
                           GString        *string)
{
  const SvgTextDecoration *d = (const SvgTextDecoration *)value;
  gboolean has_value = FALSE;
  for (size_t i = 0; i < G_N_ELEMENTS (text_decorations); i++)
    if (d->value & text_decorations[i].value)
      {
        if (has_value)
          g_string_append_c (string, ' ');
        g_string_append (string, text_decorations[i].name);
        has_value = TRUE;
      }
}

static SvgValue * svg_text_decoration_resolve (const SvgValue    *value,
                                               ShapeAttr          attr,
                                               unsigned int       idx,
                                               Shape             *shape,
                                               SvgComputeContext *context);

static const SvgValueClass SVG_TEXT_DECORATION_CLASS = {
  "SvgTextDecoration",
  svg_value_default_free,
  svg_text_decoration_equal,
  svg_text_decoration_interpolate,
  svg_text_decoration_accumulate,
  svg_text_decoration_print,
  svg_value_default_distance,
  svg_text_decoration_resolve,
};

SvgValue *
svg_text_decoration_new (TextDecoration decoration)
{
  static SvgTextDecoration none = { { &SVG_TEXT_DECORATION_CLASS, 0 }, .value = TEXT_DECORATION_NONE };
  SvgTextDecoration *result;

  if (decoration == TEXT_DECORATION_NONE)
    return (SvgValue *) &none;

  result = (SvgTextDecoration *) svg_value_alloc (&SVG_TEXT_DECORATION_CLASS, sizeof (SvgTextDecoration));
  result->value = decoration;
  return (SvgValue *) result;
}

SvgValue *
svg_text_decoration_parse (GtkCssParser *parser)
{
  TextDecoration val = TEXT_DECORATION_NONE;

  while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
    {
      unsigned int i;

      gtk_css_parser_skip_whitespace (parser);

      for (i = 0; i < G_N_ELEMENTS (text_decorations); i++)
        {
          if (gtk_css_parser_try_ident (parser, text_decorations[i].name))
            {
              val |= text_decorations[i].value;
              break;
            }
        }

      if (i == G_N_ELEMENTS (text_decorations))
        return NULL;
    }

  return svg_text_decoration_new (val);
}

static SvgValue *
svg_text_decoration_resolve (const SvgValue    *value,
                             ShapeAttr          attr,
                             unsigned int       idx,
                             Shape             *shape,
                             SvgComputeContext *context)
{
  TextDecoration ret;

  if (!context->parent)
    return svg_value_ref ((SvgValue *)value);

  ret = svg_text_decoration_get (context->parent->current[attr]);
  if (ret == TEXT_DECORATION_NONE)
    return svg_value_ref ((SvgValue *)value);

  ret |= svg_text_decoration_get (value);

  return svg_text_decoration_new (ret);
}

TextDecoration
svg_text_decoration_get (const SvgValue *value)
{
  const SvgTextDecoration *d = (const SvgTextDecoration *)value;
  return d->value;
}

