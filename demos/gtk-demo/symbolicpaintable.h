#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SYMBOLIC_TYPE_PAINTABLE (symbolic_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SymbolicPaintable, symbolic_paintable, SYMBOLIC, PAINTABLE, GObject)

GdkPaintable * symbolic_paintable_new   (GFile *file);

G_END_DECLS
