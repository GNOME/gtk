#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgvalueprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gtk/gtkenums.h"

G_BEGIN_DECLS

typedef enum
{
  COLOR_CURRENT,
  COLOR_SYMBOLIC,
  COLOR_PLAIN,
} ColorKind;

SvgValue * svg_color_new_symbolic    (GtkSymbolicColor  symbolic);
SvgValue * svg_color_new_current     (void);
SvgValue * svg_color_new_black       (void);
SvgValue * svg_color_new_transparent (void);
SvgValue * svg_color_new_color       (const GdkColor   *color);
SvgValue * svg_color_new_rgba        (const GdkRGBA    *rgba);
SvgValue * svg_color_parse           (GtkCssParser     *parser);

ColorKind  svg_color_get_kind        (const SvgValue   *value);
GtkSymbolicColor
           svg_color_get_symbolic    (const SvgValue   *value);
const GdkColor *
           svg_color_get_color       (const SvgValue   *value);

const char *symbolic_system_color    (GtkSymbolicColor c);

G_END_DECLS
