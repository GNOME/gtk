#pragma once

#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_content_fit_new_none    (void);
SvgValue *   svg_content_fit_new         (Align           align_x,
                                          Align           align_y,
                                          MeetOrSlice     meet);
SvgValue *   svg_content_fit_parse       (GtkCssParser   *parser);

gboolean     svg_content_fit_is_none     (const SvgValue *value);
Align        svg_content_fit_get_align_x (const SvgValue *value);
Align        svg_content_fit_get_align_y (const SvgValue *value);
MeetOrSlice  svg_content_fit_get_meet    (const SvgValue *value);

G_END_DECLS
