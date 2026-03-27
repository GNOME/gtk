#pragma once

#include "gdk/gdktexture.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_href_new_none     (void);
SvgValue *   svg_href_new_plain    (const char     *string);
SvgValue *   svg_href_new_url_take (const char     *string);

const char * svg_href_get_id       (const SvgValue *value);
HrefKind     svg_href_get_kind     (const SvgValue *value);
Shape *      svg_href_get_shape    (const SvgValue *value);
void         svg_href_set_shape    (SvgValue       *value,
                                    Shape          *shape);
GdkTexture * svg_href_get_texture  (const SvgValue *value);
void         svg_href_set_texture  (SvgValue       *value,
                                    GdkTexture     *texture);


G_END_DECLS
