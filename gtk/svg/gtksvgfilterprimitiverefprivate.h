#pragma once

#include "gtksvgvalueprivate.h"
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef enum
{
  DEFAULT_SOURCE,
  SOURCE_GRAPHIC,
  SOURCE_ALPHA,
  BACKGROUND_IMAGE,
  BACKGROUND_ALPHA,
  FILL_PAINT,
  STROKE_PAINT,
  PRIMITIVE_REF,
} SvgFilterPrimitiveRefType;

SvgValue * svg_filter_primitive_ref_new     (SvgFilterPrimitiveRefType  type);
SvgValue * svg_filter_primitive_ref_new_ref (const char                *ref);
SvgValue * svg_filter_primitive_ref_parse   (GtkCssParser              *parser);

SvgFilterPrimitiveRefType  svg_filter_primitive_ref_get_type  (const SvgValue *value);
const char *               svg_filter_primitive_ref_get_ref   (const SvgValue *value);

G_END_DECLS
