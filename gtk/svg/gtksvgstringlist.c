#include "gtksvgstringlistprivate.h"
#include "gtksvgvalueprivate.h"

typedef struct
{
  SvgValue base;
  char separator;
  unsigned int len;
  char *values[1];
} SvgStringList;

static unsigned int
svg_string_list_size (unsigned int n)
{
  return sizeof (SvgStringList) + MAX (n - 1, 0) * sizeof (char *);
}

static void
svg_string_list_free (SvgValue *value)
{
  SvgStringList *s = (SvgStringList *)value;
  for (size_t i = 0; i < s->len; i++)
    g_free (s->values[i]);
  g_free (s);
}

static gboolean
svg_string_list_equal (const SvgValue *value0,
                       const SvgValue *value1)
{
  const SvgStringList *s0 = (const SvgStringList *)value0;
  const SvgStringList *s1 = (const SvgStringList *)value1;

  if (s0->len != s1->len)
    return FALSE;

  if (s0->separator != s1->separator)
    return FALSE;

  for (unsigned int i = 0; i < s0->len; i++)
    {
      if (strcmp (s0->values[i], s1->values[i]) != 0)
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_string_list_interpolate (const SvgValue    *value0,
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
svg_string_list_accumulate (const SvgValue    *value0,
                            const SvgValue    *value1,
                            SvgComputeContext *context,
                            int                n)
{
  return svg_value_ref ((SvgValue *)value0);
}

static void
svg_string_list_print (const SvgValue *value,
                       GString        *string)
{
  const SvgStringList *s = (const SvgStringList *)value;

  for (unsigned int i = 0; i < s->len; i++)
    {
      char *escaped = g_markup_escape_text (s->values[i], strlen (s->values[i]));
      if (i > 0)
        g_string_append_c (string, s->separator);
      g_string_append (string, escaped);
      g_free (escaped);
    }
}

static const SvgValueClass SVG_STRING_LIST_CLASS = {
  "SvgStringList",
  svg_string_list_free,
  svg_string_list_equal,
  svg_string_list_interpolate,
  svg_string_list_accumulate,
  svg_string_list_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_string_list_new (GStrv strv)
{
  static SvgStringList empty = { { &SVG_STRING_LIST_CLASS, 0 }, .len = 0, .values[0] = NULL, };
  SvgStringList *result;
  unsigned int len;

  if (strv)
    len = g_strv_length (strv);
  else
    len = 0;

  if (len == 0)
    return svg_value_ref ((SvgValue *) &empty);

  result = (SvgStringList *) svg_value_alloc (&SVG_STRING_LIST_CLASS, svg_string_list_size (len));

  result->len = len;
  for (unsigned int i = 0; i < len; i++)
    result->values[i] = g_strdup (strv[i]);

  result->separator = ' ';

  return (SvgValue *) result;
}

SvgValue *
svg_string_list_new_take (GStrv strv,
                          char  separator)
{
  SvgStringList *result;
  unsigned int len = g_strv_length (strv);

  result = (SvgStringList *) svg_value_alloc (&SVG_STRING_LIST_CLASS, svg_string_list_size (len));

  result->len = len;
  for (unsigned int i = 0; i < len; i++)
    result->values[i] = strv[i];

  g_free (strv);

  result->separator = separator;

  return (SvgValue *) result;
}

unsigned int
svg_string_list_get_length (const SvgValue *value)
{
  const SvgStringList *s = (const SvgStringList *) value;

  g_assert (value->class == &SVG_STRING_LIST_CLASS);

  return s->len;
}

const char *
svg_string_list_get (const SvgValue *value,
                     unsigned int    pos)
{
  const SvgStringList *s = (const SvgStringList *) value;

  g_assert (value->class == &SVG_STRING_LIST_CLASS);
  g_assert (pos < s->len);

  return s->values[pos];
}
