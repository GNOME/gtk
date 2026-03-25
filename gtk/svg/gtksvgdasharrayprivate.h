#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *    svg_dash_array_new_none   (void);
SvgValue *    svg_dash_array_new        (double         *values,
                                         unsigned int    n_values);
SvgValue *    svg_dash_array_parse      (GtkCssParser   *parser);

DashArrayKind svg_dash_array_get_kind   (const SvgValue *value);
unsigned int  svg_dash_array_get_length (const SvgValue *value);
SvgUnit       svg_dash_array_get_unit   (const SvgValue *value,
                                         unsigned int    pos);
double        svg_dash_array_get        (const SvgValue *value,
                                         unsigned int    pos,
                                         double          one_hundred_percent);

G_END_DECLS
