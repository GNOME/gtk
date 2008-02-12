#ifndef MITYPES_H
#define MITYPES_H

#include <alloca.h>

#define ALLOCATE_LOCAL(size) alloca((int)(size))
#define DEALLOCATE_LOCAL(ptr)  /* as nothing */

#include <stdlib.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gdkfb.h>
#include <gdkprivate-fb.h>
#include <gdkregion-generic.h>

typedef struct _miDash *miDashPtr;

#define CT_NONE                 0
#define CT_PIXMAP               1
#define CT_REGION               2
#define CT_UNSORTED             6
#define CT_YSORTED              10
#define CT_YXSORTED             14
#define CT_YXBANDED             18

#define PixmapBytePad(w, d) (w)
#define BitmapBytePad(w) (w)

typedef struct _miArc {
    gint16 x, y;
    guint16   width, height;
    gint16   angle1, angle2;
} miArc;

#define SCRRIGHT(x, n) ((x)>>(n))

#endif /* MITYPES_H */
