#include "gdk.h"
#include "gdkinternals.h"
#include "visual.h"

#include <xcb/xcb.h>

typedef GdkScreenClass GdkScreenXcbClass;

typedef struct
{
  GdkScreen parent_instance;

  GdkVisual *system_visual;
  GdkWindow *root_window;
  GdkDisplay *display;

  xcb_screen_t *scr;
  gint number;
} GdkScreenXcb;

static GType gdk_screen_xcb_get_type (void);
G_DEFINE_TYPE (GdkScreenXcb, gdk_screen_xcb, GDK_TYPE_SCREEN)

static void
gdk_screen_xcb_init (GdkScreenXcb *scrxcb)
{
}

static void
gdk_screen_xcb_class_init (GdkScreenXcbClass *class)
{
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
                        const gchar *name,
                        GValue      *value)
{
  return FALSE;
}

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  GdkScreenXcb *scrxcb = (GdkScreenXcb *) screen;

  return scrxcb->display;
}

int
gdk_screen_get_number (GdkScreen *screen)
{
  GdkScreenXcb *scrxcb = (GdkScreenXcb *) screen;

  return scrxcb->number;
}

GdkVisual *
gdk_screen_get_system_visual (GdkScreen *screen)
{
  GdkScreenXcb *scrxcb = (GdkScreenXcb *) screen;

  return scrxcb->system_visual;
}

GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  GdkScreenXcb *scrxcb = (GdkScreenXcb *) screen;

  return scrxcb->root_window;
}

GdkScreenXcb *
gdk_screen_xcb_new (GdkDisplay   *display,
                    gint          number,
                    xcb_screen_t *scr)
{
  GdkWindowObject *private;
  GdkScreenXcb *scrxcb;

  scrxcb = g_object_new (gdk_screen_xcb_get_type (), NULL);
  scrxcb->system_visual = gdk_visual_xcb_new (GDK_SCREEN (scrxcb));
  scrxcb->root_window = g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *) scrxcb->root_window;
  private->visual = scrxcb->system_visual;
  private->window_type = GDK_WINDOW_ROOT;
  scrxcb->display = display;
  scrxcb->number = number;
  scrxcb->scr = scr;

  return scrxcb;
}
