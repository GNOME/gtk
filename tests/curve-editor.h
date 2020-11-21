#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CURVE_TYPE_EDITOR (curve_editor_get_type ())
G_DECLARE_FINAL_TYPE (CurveEditor, curve_editor, CURVE, EDITOR, GtkWidget)

GtkWidget * curve_editor_new      (void);

void        curve_editor_set_edit (CurveEditor *self,
                                   gboolean     edit);

void        curve_editor_set_path (CurveEditor *self,
                                   GskPath     *path);

G_END_DECLS
