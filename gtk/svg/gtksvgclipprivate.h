#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgprivate.h"
#include "gsk/gskenums.h"
#include "gsk/gskpath.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_clip_new_none      (void);
SvgValue *   svg_clip_new_path      (const char     *string,
                                     unsigned int    fill_rule);
SvgValue *   svg_clip_new_url_take  (const char     *string);

SvgValue *   svg_clip_parse         (GtkCssParser   *parser);

ClipKind     svg_clip_get_kind      (const SvgValue *value);
const char * svg_clip_get_id        (const SvgValue *value);
Shape *      svg_clip_get_shape     (const SvgValue *value);
GskFillRule  svg_clip_get_fill_rule (const SvgValue *value);
GskPath *    svg_clip_get_path      (const SvgValue *value);

void         svg_clip_set_shape     (SvgValue       *value,
                                     Shape          *shape);

G_END_DECLS
