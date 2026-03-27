#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include <graphene.h>
#include "gtksvgprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

SvgValue *   svg_filter_new_none         (void);
SvgValue *   svg_filter_parse_css        (GtkCssParser      *parser);
SvgValue *   svg_filter_parse            (const char        *value);

gboolean     svg_filter_is_none          (const SvgValue    *value);

unsigned int svg_filter_get_length       (const SvgValue    *value);
FilterKind   svg_filter_get_kind         (const SvgValue    *value,
                                          unsigned int       pos);
const char * svg_filter_get_ref          (const SvgValue    *value,
                                          unsigned int       pos);
Shape *      svg_filter_get_shape        (const SvgValue    *value,
                                          unsigned int       pos);
void         svg_filter_set_shape        (SvgValue          *value,
                                          unsigned int       pos,
                                          Shape             *shape);
double       svg_filter_get_simple       (const SvgValue    *value,
                                          unsigned int       pos);
void         svg_filter_get_color_matrix (const SvgValue    *value,
                                          unsigned int       pos,
                                          graphene_matrix_t *matrix,
                                          graphene_vec4_t   *offset);
void         svg_filter_get_dropshadow   (const SvgValue    *value,
                                          unsigned int       pos,
                                          GdkColor          *color,
                                          double            *dx,
                                          double            *dy,
                                          double            *std_dev);

gboolean     svg_filter_needs_backdrop   (const SvgValue    *value);

void color_matrix_type_get_color_matrix  (ColorMatrixType    type,
                                          SvgValue          *values,
                                          graphene_matrix_t *matrix,
                                          graphene_vec4_t   *offset);

gboolean filter_needs_backdrop           (Shape             *filter);


G_END_DECLS
