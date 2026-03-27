#pragma once

#include "gtksvgvalueprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgprivate.h"
#include <graphene.h>

G_BEGIN_DECLS

typedef enum
{
  SVG_PARSE_NUMBER     = 1 << 0,
  SVG_PARSE_PERCENTAGE = 1 << 1,
  SVG_PARSE_LENGTH     = 1 << 2,
  SVG_PARSE_ANGLE      = 1 << 3,
} SvgNumberParseFlags;

SvgValue * svg_number_new               (double               value);
SvgValue * svg_number_new_full          (SvgUnit              unit,
                                         double               value);

SvgValue * svg_percentage_new           (double               value);
gboolean   svg_number_parse2            (GtkCssParser        *parser,
                                         double               min,
                                         double               max,
                                         SvgNumberParseFlags  flags,
                                         double              *d,
                                         SvgUnit             *u);

SvgValue * svg_number_parse             (GtkCssParser        *parser,
                                         double               min,
                                         double               max,
                                         SvgNumberParseFlags  flags);

SvgUnit    svg_number_get_unit          (const SvgValue      *value);
double     svg_number_get               (const SvgValue      *value,
                                         double               one_hundred_percent);
gboolean   svg_value_is_number          (const SvgValue      *value);
gboolean   svg_value_is_positive_number (const SvgValue      *value);


const char *svg_unit_name (SvgUnit unit);


gboolean is_absolute_length          (SvgUnit                unit);

double   absolute_length_to_px       (double                 value,
                                      SvgUnit                unit);

double   shape_get_current_font_size (Shape                 *shape,
                                      ShapeAttr              attr,
                                      SvgComputeContext     *context);
double   angle_to_deg                (double                 value,
                                      SvgUnit                unit);
double   viewport_relative_to_px     (double                 value,
                                      SvgUnit                unit,
                                      const graphene_rect_t *viewport);

G_END_DECLS
