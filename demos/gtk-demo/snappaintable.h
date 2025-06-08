#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SNAP_TYPE_PAINTABLE (snap_paintable_get_type ())

G_DECLARE_FINAL_TYPE (SnapPaintable, snap_paintable, SNAP, PAINTABLE, GObject)

SnapPaintable * snap_paintable_new      (GFile *file);

void            snap_paintable_set_file (SnapPaintable *self,
                                         GFile         *file);
GskRectSnap     snap_paintable_get_snap (SnapPaintable *self);
void            snap_paintable_set_snap (SnapPaintable *self,
                                         GskRectSnap    snap);
int             snap_paintable_get_zoom (SnapPaintable *self);
void            snap_paintable_set_zoom (SnapPaintable *self,
                                         int            zoom);
gboolean        snap_paintable_get_tiles (SnapPaintable *self);
void            snap_paintable_set_tiles (SnapPaintable *self,
                                          gboolean       tiles);

G_END_DECLS
