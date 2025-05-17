#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SVG_TYPE_SYMBOLIC_PAINTABLE (svg_symbolic_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SvgSymbolicPaintable, svg_symbolic_paintable, SVG, SYMBOLIC_PAINTABLE, GObject)

SvgSymbolicPaintable * svg_symbolic_paintable_new   (GFile *file);

G_END_DECLS
