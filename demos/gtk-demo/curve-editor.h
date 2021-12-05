#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CURVE_TYPE_EDITOR (curve_editor_get_type ())
G_DECLARE_FINAL_TYPE (CurveEditor, curve_editor, CURVE, EDITOR, GtkWidget)

GtkWidget *       curve_editor_new              (void);

void              curve_editor_set_edit         (CurveEditor *self,
                                                 gboolean     edit);

void              curve_editor_set_path         (CurveEditor *self,
                                                 GskPath     *path);

GskPath *         curve_editor_get_path         (CurveEditor *self);

void              curve_editor_set_stroke       (CurveEditor *self,
                                                 GskStroke   *stroke);

const GskStroke * curve_editor_get_stroke       (CurveEditor *self);


void              curve_editor_set_color        (CurveEditor *self,
                                                 GdkRGBA     *color);

const GdkRGBA *   curve_editor_get_color        (CurveEditor *self);

gboolean          curve_editor_get_show_outline (CurveEditor *self);

void              curve_editor_set_show_outline (CurveEditor *self,
                                                 gboolean     show_outline);

G_END_DECLS
