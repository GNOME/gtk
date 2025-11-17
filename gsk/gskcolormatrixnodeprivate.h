#pragma once

#include "gskcolormatrixnode.h"

G_BEGIN_DECLS

void apply_color_matrix_to_pattern (cairo_pattern_t         *pattern,
                                    const graphene_matrix_t *color_matrix,
                                    const graphene_vec4_t   *color_offset);

G_END_DECLS
