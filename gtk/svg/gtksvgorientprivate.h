#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtksvgprivate.h"

G_BEGIN_DECLS

SvgValue *  svg_orient_new_angle         (double          angle,
                                          SvgUnit         unit);
SvgValue *  svg_orient_new_auto          (gboolean        start_reverse);
SvgValue *  svg_orient_parse             (GtkCssParser   *parser);

OrientKind  svg_orient_get_kind          (const SvgValue *value);
gboolean    svg_orient_get_start_reverse (const SvgValue *value);
double      svg_orient_get_angle         (const SvgValue *value);

G_END_DECLS
