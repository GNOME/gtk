#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gdk/gdkcolorprivate.h"
#include "gtksvgprivate.h"
#include "gtkenums.h"
#include "gdkrgba.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

SvgValue * svg_paint_new_none        (void);
SvgValue * svg_paint_new_symbolic    (GtkSymbolicColor  symbolic);
SvgValue * svg_paint_new_rgba        (const GdkRGBA    *rgba);
SvgValue * svg_paint_new_color       (const GdkColor   *color);
SvgValue * svg_paint_new_black       (void);
SvgValue * svg_paint_new_transparent (void);
SvgValue * svg_paint_new_simple      (PaintKind         kind);
SvgValue * svg_paint_new_server      (const char       *ref);
SvgValue * svg_paint_new_server_with_fallback
                                     (const char       *ref,
                                      const GdkColor   *fallback);
SvgValue * svg_paint_new_server_with_current_color
                                     (const char       *ref);

SvgValue * svg_paint_parse           (GtkCssParser     *parser);
SvgValue * svg_paint_parse_gpa       (const char       *value);
void       svg_paint_print_gpa       (const SvgValue   *value,
                                      GString          *string);

gboolean     svg_value_is_paint            (const SvgValue   *value);
PaintKind    svg_paint_get_kind            (const SvgValue   *value);
GtkSymbolicColor
             svg_paint_get_symbolic        (const SvgValue   *value);
gboolean     svg_paint_is_symbolic         (const SvgValue   *value,
                                            GtkSymbolicColor *symbolic);
const GdkColor *
             svg_paint_get_color           (const SvgValue   *value);
const char * svg_paint_get_server_ref      (const SvgValue   *value);
Shape *      svg_paint_get_server_shape    (const SvgValue *value);
void         svg_paint_set_server_shape    (SvgValue         *value,
                                            Shape            *shape);
const GdkColor *
             svg_paint_get_server_fallback (const SvgValue   *value);

gboolean     paint_is_server               (PaintKind         kind);
gboolean     parse_symbolic_color          (const char       *string,
                                            GtkSymbolicColor *sym);

G_END_DECLS
