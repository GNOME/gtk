#include <xcb/xcb.h>

GdkScreen *
gdk_screen_xcb_new (GdkDisplay   *display,
                    gint          number,
                    xcb_screen_t *scr);
