#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_numbers_new                 (double       *values,
                                              unsigned int  n_values);

SvgValue *   svg_numbers_new_identity_matrix (void);

SvgValue *   svg_numbers_new_none            (void);

SvgValue *   svg_numbers_new1                (double value);

SvgValue *   svg_numbers_new2                (double v1, SvgUnit u1,
                                              double v2, SvgUnit u2);

SvgValue *   svg_numbers_new_00              (void);

SvgValue *   svg_numbers_parse               (GtkCssParser *parser);
SvgValue *   svg_numbers_parse2              (GtkCssParser *parser,
                                              unsigned int  flags);

unsigned int svg_numbers_get_length          (const SvgValue     *value);
SvgUnit      svg_numbers_get_unit            (const SvgValue     *value,
                                              unsigned int        pos);
double       svg_numbers_get                 (const SvgValue     *value,
                                              unsigned int        pos,
                                              double              one_hundred_percent);

SvgValue *   svg_numbers_drop_last           (const SvgValue *value);

G_END_DECLS
