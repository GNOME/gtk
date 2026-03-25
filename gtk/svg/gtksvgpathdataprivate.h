#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

gboolean      svg_path_data_parse_full  (const char   *string,
                                         SvgPathData **path_data);
SvgPathData * svg_path_data_parse       (const char  *string);
void          svg_path_data_free        (SvgPathData  *path_data);

SvgPathData * svg_path_data_from_gsk    (GskPath      *path);
GskPath *     svg_path_data_to_gsk      (SvgPathData  *path_data);

SvgPathData * svg_path_data_interpolate (SvgPathData  *path_data0,
                                         SvgPathData  *path_data1,
                                         double        t);

void svg_path_data_print                (SvgPathData  *path_data,
                                         GString      *string);
