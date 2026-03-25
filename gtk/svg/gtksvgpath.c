#include "gtksvgpathprivate.h"
#include "gtksvgvalueprivate.h"
#include "gsk/gskpathprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgpathdataprivate.h"


typedef struct
{
  SvgValue base;
  SvgPathData *pdata;
  GskPath *path;
} SvgPath;

static void
svg_path_free (SvgValue *value)
{
  SvgPath *p = (SvgPath *) value;
  g_clear_pointer (&p->pdata, svg_path_data_free);
  g_clear_pointer (&p->path, gsk_path_unref);
  g_free (value);
}

static gboolean
svg_path_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgPath *p0 = (const SvgPath *) value0;
  const SvgPath *p2 = (const SvgPath *) value1;

  if (p0->path == p2->path)
    return TRUE;

  if (!p0->path || !p2->path)
    return FALSE;

  return gsk_path_equal (p0->path, p2->path);
}

static SvgValue *
svg_path_accumulate (const SvgValue    *value0,
                     const SvgValue    *value1,
                     SvgComputeContext *context,
                     int                n)
{
  return NULL;
}

static void
svg_path_print (const SvgValue *value,
                GString        *string)
{
  const SvgPath *p = (const SvgPath *) value;

  if (p->pdata)
    svg_path_data_print (p->pdata, string);
  else
    g_string_append (string, "none");
}

static SvgValue * svg_path_interpolate (const SvgValue    *value1,
                                        const SvgValue    *value2,
                                        SvgComputeContext *context,
                                        double             t);

static const SvgValueClass SVG_PATH_CLASS = {
  "SvgPath",
  svg_path_free,
  svg_path_equal,
  svg_path_interpolate,
  svg_path_accumulate,
  svg_path_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_path_new_none (void)
{
  static SvgPath none = { { &SVG_PATH_CLASS, 0 }, NULL, NULL };

  return (SvgValue *) &none;
}

SvgValue *
svg_path_new_from_data (SvgPathData *pdata)
{
  SvgPath *result;

  if (pdata == NULL)
    return NULL;

  result = (SvgPath *) svg_value_alloc (&SVG_PATH_CLASS, sizeof (SvgPath));
  result->pdata = pdata;
  result->path = svg_path_data_to_gsk (pdata);

  return (SvgValue *) result;
}

SvgValue *
svg_path_new (GskPath *path)
{
  return svg_path_new_from_data (svg_path_data_from_gsk (path));
}

static guint
path_arg (GtkCssParser *parser,
          guint         n,
          gpointer      data)
{
  char **string = data;

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_STRING))
    return 0;

  *string = gtk_css_parser_consume_string (parser);
  return 1;
}

SvgValue *
svg_path_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_path_new_none ();
  else if (gtk_css_parser_has_function (parser, "path"))
    {
      char *string = NULL;
      GtkCssLocation start;

      start = *gtk_css_parser_get_start_location (parser);
      if (gtk_css_parser_consume_function (parser, 1, 1, path_arg, &string))
        {
          SvgValue *value;
          SvgPathData *data = NULL;

          if (!svg_path_data_parse_full (string, &data))
            {
              GError *error;

              error = g_error_new_literal (GTK_CSS_PARSER_ERROR,
                                           GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                                           "Path data is invalid");
              gtk_css_parser_emit_error (parser,
                                         &start,
                                         gtk_css_parser_get_end_location (parser),
                                         error);
              g_error_free (error);
            }

          value = svg_path_new_from_data (data);

          g_free (string);

          return value;
        }
    }

  gtk_css_parser_error_syntax (parser, "Expected a path");
  return NULL;
}

GskPath *
svg_path_get_gsk (const SvgValue *value)
{
  const SvgPath *p = (const SvgPath *) value;

  g_assert (value->class == &SVG_PATH_CLASS);

  return p->path;
}

static SvgValue *
svg_path_interpolate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      double             t)
{
  const SvgPath *p0 = (const SvgPath *) value0;
  const SvgPath *p1 = (const SvgPath *) value1;
  SvgPathData *p;

  p = svg_path_data_interpolate (p0->pdata, p1->pdata, t);

  if (p != NULL)
    return svg_path_new_from_data (p);

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}
