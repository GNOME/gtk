#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtksvgtypesprivate.h"

G_BEGIN_DECLS

SvgValue * svg_path_new           (GskPath        *path);
SvgValue * svg_path_new_none      (void);
SvgValue * svg_path_new_from_data (SvgPathData    *path_data);

SvgValue * svg_path_parse         (GtkCssParser   *parser);

GskPath *  svg_path_get_gsk       (const SvgValue *value);

G_END_DECLS
