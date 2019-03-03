

#ifndef __GTK_FOCUS_OVERLAY_H__
#define __GTK_FOCUS_OVERLAY_H__

#include "inspectoroverlay.h"

G_BEGIN_DECLS

#define GTK_TYPE_FOCUS_OVERLAY             (gtk_focus_overlay_get_type ())
G_DECLARE_FINAL_TYPE (GtkFocusOverlay, gtk_focus_overlay, GTK, FOCUS_OVERLAY, GtkInspectorOverlay)

GtkInspectorOverlay *   gtk_focus_overlay_new                 (void);

G_END_DECLS



#endif
