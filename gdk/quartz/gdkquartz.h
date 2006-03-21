#ifndef __GDK_QUARTZ_H__
#define __GDK_QUARTZ_H__

#include <Quartz/Quartz.h>
#include "gdk/gdkprivate.h"
#include "gdkprivate-quartz.h"
#include "gdkdrawable-quartz.h"
#include "gdkwindow-quartz.h"

G_BEGIN_DECLS

NSView *gdk_quartz_window_get_nsview (GdkWindow *window);

G_END_DECLS

#endif /* __GDK_QUARTZ_H__ */
