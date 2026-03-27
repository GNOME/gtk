#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_mask_new_none     (void);
SvgValue *   svg_mask_new_url_take (const char     *string);
SvgValue *   svg_mask_parse        (GtkCssParser   *parser);

MaskKind     svg_mask_get_kind     (const SvgValue *value);
const char * svg_mask_get_id       (const SvgValue *value);
Shape *      svg_mask_get_shape    (const SvgValue *value);
void         svg_mask_set_shape    (SvgValue       *mask,
                                    Shape          *shape);

G_END_DECLS
