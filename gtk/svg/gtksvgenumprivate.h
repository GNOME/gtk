#pragma once

#include "gdk/gdkenums.h"
#include "gsk/gskenums.h"
#include "gsk/gskgradientprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtk/css/gtkcssparserprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_fill_rule_new               (GskFillRule           value);
SvgValue *   svg_mask_type_new               (GskMaskMode           value);
SvgValue *   svg_linecap_new                 (GskLineCap            value);
SvgValue *   svg_linejoin_new                (GskLineJoin           value);
SvgValue *   svg_visibility_new              (Visibility            value);
SvgValue *   svg_display_new                 (SvgDisplay            value);
SvgValue *   svg_spread_method_new           (GskRepeat             value);
SvgValue *   svg_coord_units_new             (CoordUnits            value);
SvgValue *   svg_paint_order_new             (PaintOrder            value);
SvgValue *   svg_overflow_new                (SvgOverflow           value);
SvgValue *   svg_isolation_new               (Isolation             value);
SvgValue *   svg_blend_mode_new              (GskBlendMode          value);
SvgValue *   svg_text_anchor_new             (TextAnchor            value);
SvgValue *   svg_marker_units_new            (MarkerUnits           value);
SvgValue *   svg_unicode_bidi_new            (UnicodeBidi           value);
SvgValue *   svg_direction_new               (PangoDirection        value);
SvgValue *   svg_writing_mode_new            (WritingMode           value);
SvgValue *   svg_font_style_new              (PangoStyle            value);
SvgValue *   svg_font_variant_new            (PangoVariant          value);
SvgValue *   svg_font_stretch_new            (PangoStretch          value);
SvgValue *   svg_font_size_new               (FontSize              value);
SvgValue *   svg_font_weight_new             (FontWeight            value);
SvgValue *   svg_edge_mode_new               (EdgeMode              value);
SvgValue *   svg_blend_composite_new         (BlendComposite        value);
SvgValue *   svg_color_matrix_type_new       (ColorMatrixType       value);
SvgValue *   svg_composite_operator_new      (CompositeOperator     value);
SvgValue *   svg_rgba_channel_new            (GdkColorChannel       value);
SvgValue *   svg_component_transfer_type_new (ComponentTransferType value);
SvgValue *   svg_vector_effect_new           (VectorEffect          value);
SvgValue *   svg_color_interpolation_new     (ColorInterpolation    value);
SvgValue *   svg_transform_box_new           (TransformBox          value);
SvgValue *   svg_shape_rendering_new         (ShapeRendering        value);
SvgValue *   svg_text_rendering_new          (TextRendering         value);
SvgValue *   svg_image_rendering_new         (ImageRendering        value);

unsigned int svg_enum_get (const SvgValue *value);

SvgValue *   svg_fill_rule_parse               (GtkCssParser     *parser);
SvgValue *   svg_mask_type_parse               (GtkCssParser     *parser);
SvgValue *   svg_linecap_parse                 (GtkCssParser     *parser);
SvgValue *   svg_linejoin_parse                (GtkCssParser     *parser);
SvgValue *   svg_visibility_parse              (GtkCssParser     *parser);
SvgValue *   svg_display_parse                 (GtkCssParser     *parser);
SvgValue *   svg_spread_method_parse           (GtkCssParser     *parser);
SvgValue *   svg_coord_units_parse             (GtkCssParser     *parser);
SvgValue *   svg_paint_order_parse             (GtkCssParser     *parser);
SvgValue *   svg_overflow_parse                (GtkCssParser     *parser);
SvgValue *   svg_isolation_parse               (GtkCssParser     *parser);
SvgValue *   svg_blend_mode_parse              (GtkCssParser     *parser);
SvgValue *   svg_text_anchor_parse             (GtkCssParser     *parser);
SvgValue *   svg_marker_units_parse            (GtkCssParser     *parser);
SvgValue *   svg_unicode_bidi_parse            (GtkCssParser     *parser);
SvgValue *   svg_direction_parse               (GtkCssParser     *parser);
SvgValue *   svg_writing_mode_parse            (GtkCssParser     *parser);
SvgValue *   svg_font_style_parse              (GtkCssParser     *parser);
SvgValue *   svg_font_variant_parse            (GtkCssParser     *parser);
SvgValue *   svg_font_stretch_parse            (GtkCssParser     *parser);
SvgValue *   svg_font_size_parse               (GtkCssParser     *parser);
SvgValue *   svg_font_weight_parse             (GtkCssParser     *parser);
SvgValue *   svg_edge_mode_parse               (GtkCssParser     *parser);
SvgValue *   svg_blend_composite_parse         (GtkCssParser     *parser);
SvgValue *   svg_color_matrix_type_parse       (GtkCssParser     *parser);
SvgValue *   svg_composite_operator_parse      (GtkCssParser     *parser);
SvgValue *   svg_rgba_channel_parse            (GtkCssParser     *parser);
SvgValue *   svg_component_transfer_type_parse (GtkCssParser     *parser);
SvgValue *   svg_vector_effect_parse           (GtkCssParser     *parser);
SvgValue *   svg_color_interpolation_parse     (GtkCssParser     *parser);
SvgValue *   svg_transform_box_parse           (GtkCssParser     *parser);
SvgValue *   svg_shape_rendering_parse         (GtkCssParser     *parser);
SvgValue *   svg_text_rendering_parse          (GtkCssParser     *parser);
SvgValue *   svg_image_rendering_parse         (GtkCssParser     *parser);

G_END_DECLS
