#include "gtksvgfilterprimitiverefprivate.h"
#include "gtksvgvalueprivate.h"

typedef struct
{
  SvgValue base;
  SvgFilterPrimitiveRefType type;
  const char *ref;
} SvgFilterPrimitiveRef;

static void
svg_filter_primitive_ref_free (SvgValue *value)
{
  SvgFilterPrimitiveRef *f = (SvgFilterPrimitiveRef *) value;

  if (f->type == PRIMITIVE_REF)
    g_free ((gpointer) f->ref);

  g_free (f);
}

static gboolean
svg_filter_primitive_ref_equal (const SvgValue *value0,
                                const SvgValue *value1)
{
  const SvgFilterPrimitiveRef *f0 = (SvgFilterPrimitiveRef *) value0;
  const SvgFilterPrimitiveRef *f1 = (SvgFilterPrimitiveRef *) value1;

  if (f0->type != f1->type)
    return FALSE;

  if (f0->type == PRIMITIVE_REF)
    return strcmp (f0->ref, f1->ref) == 0;

  return TRUE;
}

static SvgValue *
svg_filter_primitive_ref_interpolate (const SvgValue    *value0,
                                      const SvgValue    *value1,
                                      SvgComputeContext *context,
                                      double             t)
{
  const SvgFilterPrimitiveRef *f0 = (const SvgFilterPrimitiveRef *) value0;
  const SvgFilterPrimitiveRef *f1 = (const SvgFilterPrimitiveRef *) value1;

  if (f0->type != f1->type)
    return NULL;

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_filter_primitive_ref_accumulate (const SvgValue    *value0,
                                     const SvgValue    *value1,
                                     SvgComputeContext *context,
                                     int                n)
{
  return NULL;
}

static void
svg_filter_primitive_ref_print (const SvgValue *value,
                                GString        *string)
{
  const SvgFilterPrimitiveRef *f = (const SvgFilterPrimitiveRef *) value;

  if (f->type != DEFAULT_SOURCE)
    g_string_append (string, f->ref);
}

static const SvgValueClass SVG_FILTER_PRIMITIVE_REF_CLASS = {
  "SvgPrimitiveRef",
  svg_filter_primitive_ref_free,
  svg_filter_primitive_ref_equal,
  svg_filter_primitive_ref_interpolate,
  svg_filter_primitive_ref_accumulate,
  svg_filter_primitive_ref_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

static SvgFilterPrimitiveRef filter_primitive_ref_values[] = {
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = DEFAULT_SOURCE, .ref = NULL, },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = SOURCE_GRAPHIC, .ref = "SourceGraphic", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = SOURCE_ALPHA, .ref = "SourceAlpha", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = BACKGROUND_IMAGE, .ref = "BackgroundImage", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = BACKGROUND_ALPHA, .ref = "BackgroundAlpha", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = FILL_PAINT, .ref = "FillPaint", },
     { { &SVG_FILTER_PRIMITIVE_REF_CLASS, 0 }, .type = STROKE_PAINT, .ref = "StrokePaint", },
  };

SvgValue *
svg_filter_primitive_ref_new (SvgFilterPrimitiveRefType type)
{
  g_assert (type < PRIMITIVE_REF);

  return svg_value_ref ((SvgValue *) &filter_primitive_ref_values[type]);
}

SvgValue *
svg_filter_primitive_ref_new_ref (const char *ref)
{
  SvgFilterPrimitiveRef *f = (SvgFilterPrimitiveRef *) svg_value_alloc (&SVG_FILTER_PRIMITIVE_REF_CLASS,
                                                                        sizeof (SvgFilterPrimitiveRef));

  f->type = PRIMITIVE_REF;
  f->ref = g_strdup (ref);

  return (SvgValue *) f;
}

SvgValue *
svg_filter_primitive_ref_parse (GtkCssParser *parser)
{
  for (unsigned int i = 0; i < G_N_ELEMENTS (filter_primitive_ref_values); i++)
    {
      if (filter_primitive_ref_values[i].ref &&
          gtk_css_parser_try_ident (parser, filter_primitive_ref_values[i].ref))
        return svg_value_ref ((SvgValue *) &filter_primitive_ref_values[i]);
    }

  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    {
      const GtkCssToken *token = gtk_css_parser_get_token (parser);
      SvgValue *value;

      value = svg_filter_primitive_ref_new_ref (gtk_css_token_get_string (token));
      gtk_css_parser_skip (parser);

      return value;
    }

  gtk_css_parser_error_syntax (parser, "Expected a filter primitive ref value");
  return NULL;
}

SvgFilterPrimitiveRefType
svg_filter_primitive_ref_get_type (const SvgValue *value)
{
  const SvgFilterPrimitiveRef *f = (const SvgFilterPrimitiveRef *) value;

  g_assert (value->class == &SVG_FILTER_PRIMITIVE_REF_CLASS);

  return f->type;
}

const char *
svg_filter_primitive_ref_get_ref (const SvgValue *value)
{
  const SvgFilterPrimitiveRef *f = (const SvgFilterPrimitiveRef *) value;

  g_assert (value->class == &SVG_FILTER_PRIMITIVE_REF_CLASS);

  return f->ref;
}
