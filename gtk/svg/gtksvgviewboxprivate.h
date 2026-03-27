#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"
#include <graphene.h>

G_BEGIN_DECLS

SvgValue * svg_view_box_new       (const graphene_rect_t *box);
SvgValue * svg_view_box_new_unset (void);
SvgValue * svg_view_box_parse     (GtkCssParser    *parser);
gboolean   svg_view_box_get       (const SvgValue  *value,
                                   graphene_rect_t *vb);

G_END_DECLS
