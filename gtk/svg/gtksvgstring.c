#include "gtksvgstringprivate.h"
#include "gtksvgvalueprivate.h"

typedef struct
{
  SvgValue base;
  char *value;
} SvgString;

static void
svg_string_free (SvgValue *value)
{
  SvgString *s = (SvgString *)value;
  g_free (s->value);
  g_free (s);
}

static gboolean
svg_string_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgString *s0 = (const SvgString *)value0;
  const SvgString *s1 = (const SvgString *)value1;

  return g_strcmp0 (s0->value, s1->value) == 0;
}

static SvgValue *
svg_string_interpolate (const SvgValue    *value0,
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
svg_string_accumulate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       int                n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_string_print (const SvgValue *value,
                  GString        *string)
{
  const SvgString *s = (const SvgString *)value;
  char *escaped = g_markup_escape_text (s->value, strlen (s->value));
  g_string_append (string, escaped);
  g_free (escaped);
}

static const SvgValueClass SVG_STRING_CLASS = {
  "SvgString",
  svg_string_free,
  svg_string_equal,
  svg_string_interpolate,
  svg_string_accumulate,
  svg_string_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_string_new (const char *str)
{
  static SvgString empty = { { &SVG_STRING_CLASS, 0 }, .value = (char *) "" };
  SvgString *result;

  if (str[0] == '\0')
    return svg_value_ref ((SvgValue *) &empty);

  result = (SvgString *) svg_value_alloc (&SVG_STRING_CLASS, sizeof (SvgString));
  result->value = g_strdup (str);
  return (SvgValue *) result;
}

SvgValue *
svg_string_new_take (char *str)
{
  SvgString *result;

  result = (SvgString *) svg_value_alloc (&SVG_STRING_CLASS, sizeof (SvgString));
  result->value = str;
  return (SvgValue *) result;
}

const char *
svg_string_get (const SvgValue *value)
{
  const SvgString *s = (const SvgString *)value;
  return s->value;
}
