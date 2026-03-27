#pragma once

#include "gtksvgvalueprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_string_new      (const char     *str);
SvgValue *   svg_string_new_take (char           *str);
const char * svg_string_get      (const SvgValue *value);

G_END_DECLS
