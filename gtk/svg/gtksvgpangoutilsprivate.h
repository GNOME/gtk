#pragma once

#include "gsk/gskpath.h"
#include <pango/pango.h>

G_BEGIN_DECLS

GskPath * svg_pango_layout_to_path (PangoLayout *layout);

G_END_DECLS
