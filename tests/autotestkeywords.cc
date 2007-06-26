#include <gtk/gtk.h>

#define gdk_display gdk_display_directfb
#include <gdk/directfb/gdkdirectfb.h>
#undef gdk_display
#undef GDK_DISPLAY
#undef GDK_ROOT_WINDOW

#define gdk_display gdk_display_linuxfb
#include <gdk/linux-fb/gdkfb.h>
#undef gdk_display
#undef GDK_DISPLAY
#undef GDK_ROOT_WINDOW

#if HAVE_OBJC
#define gdk_display gdk_display_quartz
#include <gdk/quartz/gdkquartz.h>
#undef gdk_display
#undef GDK_DISPLAY
#undef GDK_ROOT_WINDOW
#endif

#define gdk_display gdk_display_win32
#include <gdk/win32/gdkwin32.h>
#undef gdk_display
#undef GDK_DISPLAY
#undef GDK_ROOT_WINDOW

#define gdk_display gdk_display_x11
#include <gdk/x11/gdkx.h>
#undef gdk_display
#undef GDK_DISPLAY
#undef GDK_ROOT_WINDOW

int main() { return 0; }
