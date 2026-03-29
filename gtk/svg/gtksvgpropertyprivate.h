#pragma once

#include <glib.h>
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

void         svg_properties_init            (void);

gboolean     svg_property_inherited         (SvgProperty     attr);
gboolean     svg_property_discrete          (SvgProperty     attr);
gboolean     svg_property_has_css           (SvgProperty     attr);
gboolean     svg_property_has_presentation  (SvgProperty     attr);
gboolean     svg_property_applies_to        (SvgProperty     attr,
                                             SvgElementType  type);
gboolean     svg_property_for_element       (SvgProperty     attr);
gboolean     svg_property_for_color_stop    (SvgProperty     attr);
gboolean     svg_property_for_filter        (SvgProperty     attr);

const char * svg_property_get_name          (SvgProperty     attr);
const char * svg_property_get_presentation  (SvgProperty     attr,
                                             SvgElementType  type);

SvgValue *   svg_property_ref_initial_value (SvgProperty     attr,
                                             SvgElementType  type,
                                             gboolean        has_parent);

gboolean     svg_property_lookup            (const char     *name,
                                             SvgElementType  type,
                                             SvgProperty    *result);

gboolean     svg_property_lookup_for_css    (const char     *name,
                                             SvgProperty    *result);

gboolean     svg_property_lookup_for_stop   (const char     *name,
                                             SvgElementType  type,
                                             SvgProperty    *result);

gboolean     svg_property_lookup_for_filter (const char     *name,
                                             SvgElementType  type,
                                             SvgFilterType   filter,
                                             SvgProperty    *result);

SvgValue *   svg_property_parse             (SvgProperty     attr,
                                             const char     *string,
                                             GError        **error);

SvgValue *   svg_property_parse_and_validate (SvgProperty    attr,
                                              const char    *string,
                                              GError       **error);

GPtrArray *  svg_property_parse_values      (SvgProperty     attr,
                                             TransformType   transform_type,
                                             const char     *string);

SvgValue *   svg_property_parse_css         (SvgProperty     attr,
                                             GtkCssParser   *parser);

gboolean     svg_attr_is_deprecated         (const char     *name);

G_END_DECLS
