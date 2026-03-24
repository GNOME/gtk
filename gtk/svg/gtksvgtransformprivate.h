#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gsk/gsktransform.h"

SvgValue * svg_transform_new_none      (void);
SvgValue * svg_transform_new_translate (double x, double y);
SvgValue * svg_transform_new_scale     (double x, double y);
SvgValue * svg_transform_new_rotate    (double angle, double x, double y);
SvgValue * svg_transform_new_skew_x    (double angle);
SvgValue * svg_transform_new_skew_y    (double angle);
SvgValue * svg_transform_new_matrix    (double params[6]);
SvgValue * svg_transform_new_rotate_and_shift
                                       (double            angle,
                                        graphene_point_t *orig,
                                        graphene_point_t *final);

SvgValue * svg_transform_parse         (const char    *value);
SvgValue * svg_transform_parse_css     (GtkCssParser  *parser);

SvgValue * primitive_transform_parse       (TransformType   type,
                                            const char     *value);
void       svg_primitive_transform_print   (const SvgValue *value,
                                            GString        *string);

GskTransform * svg_transform_get_gsk       (const SvgValue *value);

unsigned int   svg_transform_get_length    (const SvgValue *value);
SvgValue *     svg_transform_get_transform (const SvgValue *value,
                                            unsigned int    pos);
TransformType  svg_transform_get_type      (const SvgValue *value,
                                            unsigned int    pos);
gboolean       svg_transform_is_none       (const SvgValue *value);
TransformType  svg_transform_get_primitive (const SvgValue *value,
                                            unsigned int    pos,
                                            double          params[6]);
gboolean       svg_value_is_transform      (const SvgValue *value);


typedef struct
{
  double value;
  SvgUnit unit;
} ParserNumber;

unsigned int css_parser_parse_number (GtkCssParser *parser,
                                      unsigned int  n,
                                      gpointer      data);

unsigned int css_parser_parse_number_length (GtkCssParser *parser,
                                             unsigned int  n,
                                             gpointer      data);

unsigned int css_parser_parse_number_angle (GtkCssParser *parser,
                                            unsigned int  n,
                                            gpointer      data);

unsigned int css_parser_parse_number_percentage (GtkCssParser *parser,
                                                 unsigned int  n,
                                                 gpointer      data);

G_END_DECLS

