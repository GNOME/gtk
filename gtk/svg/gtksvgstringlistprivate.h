#pragma once

#include "gtksvgvalueprivate.h"

G_BEGIN_DECLS

SvgValue *    svg_string_list_new        (GStrv           strv);
SvgValue *    svg_string_list_new_take   (GStrv           strv,
                                          char            separator);

unsigned int  svg_string_list_get_length (const SvgValue *value);
const char *  svg_string_list_get        (const SvgValue *value,
                                          unsigned int    pos);

G_END_DECLS
