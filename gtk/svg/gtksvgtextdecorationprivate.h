#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue * svg_text_decoration_new (TextDecoration decoration);

SvgValue * svg_text_decoration_parse (GtkCssParser *parser);

TextDecoration svg_text_decoration_get (const SvgValue *value);

G_END_DECLS
