#include "gtksvglanguageprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtkmain.h"

typedef struct
{
  SvgValue base;
  unsigned int len;
  PangoLanguage *values[1];
} SvgLanguage;

static unsigned int
svg_language_size (unsigned int n)
{
  return sizeof (SvgLanguage) + (n > 0 ? n - 1 : 0) * sizeof (PangoLanguage *);
}

static gboolean
svg_language_equal (const SvgValue *value0,
                    const SvgValue *value1)
{
  const SvgLanguage *l0 = (const SvgLanguage *)value0;
  const SvgLanguage *l1 = (const SvgLanguage *)value1;

  if (l0->len != l1->len)
    return FALSE;

  for (unsigned int i = 0; i < l0->len; i++)
    {
      if (l0->values[i] != l1->values[i])
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_language_interpolate (const SvgValue    *value0,
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
svg_language_accumulate (const SvgValue    *value0,
                         const SvgValue    *value1,
                         SvgComputeContext *context,
                         int                n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_language_print (const SvgValue *value,
                    GString        *string)
{
  const SvgLanguage *l = (const SvgLanguage *)value;

  for (unsigned int i = 0; i < l->len; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      g_string_append (string, pango_language_to_string (l->values[i]));
    }
}

static const SvgValueClass SVG_LANGUAGE_CLASS = {
  "SvgLanguage",
  svg_value_default_free,
  svg_language_equal,
  svg_language_interpolate,
  svg_language_accumulate,
  svg_language_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_language_new_list (unsigned int    len,
                       PangoLanguage **langs)
{
  static SvgLanguage empty = { { &SVG_LANGUAGE_CLASS, 0 }, .len = 0, .values[0] = NULL, };
  SvgLanguage *result;

  if (len == 0)
    return (SvgValue *) &empty;

  result = (SvgLanguage *) svg_value_alloc (&SVG_LANGUAGE_CLASS,
                                            svg_language_size (len));

  result->len = len;

  for (unsigned int i = 0; i < len; i++)
    result->values[i] = langs[i];

  return (SvgValue *) result;
}

SvgValue *
svg_language_new (PangoLanguage *language)
{
  return svg_language_new_list (1, &language);
}

SvgValue *
svg_language_new_default (void)
{
  static SvgLanguage def = { { &SVG_LANGUAGE_CLASS, 0 }, .len = 1, .values[0] = NULL, };

  if (def.values[0] == NULL)
    def.values[0] = gtk_get_default_language ();

  return (SvgValue *) &def;
}

unsigned int
svg_language_get_length (const SvgValue *value)
{
  const SvgLanguage *l = (const SvgLanguage *)value;

  g_assert (value->class == &SVG_LANGUAGE_CLASS);

  return l->len;
}

PangoLanguage *
svg_language_get (const SvgValue *value,
                  unsigned int    pos)
{
  const SvgLanguage *l = (const SvgLanguage *)value;

  g_assert (value->class == &SVG_LANGUAGE_CLASS);
  g_assert (pos < l->len);

  return l->values[pos];
}
