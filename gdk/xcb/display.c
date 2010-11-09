#include "gdk.h"

#include "screen.h"

#include <xcb/xcb.h>

typedef GdkDisplayClass GdkDisplayXcbClass;

typedef struct
{
  GdkDisplay parent_instance;

  GdkScreen *default_screen;
  xcb_connection_t *cn;
} GdkDisplayXcb;

GType gdk_screen_xcb_get_type (void);
static GType gdk_display_xcb_get_type (void);
G_DEFINE_TYPE (GdkDisplayXcb, gdk_display_xcb, GDK_TYPE_DISPLAY)

static void
gdk_display_xcb_init (GdkDisplayXcb *dispxcb)
{
}

static void
gdk_display_xcb_class_init (GdkDisplayXcbClass *class)
{
}

GdkDisplay *
gdk_display_open (const gchar *display_name)
{
  GdkDisplayXcb *dispxcb;
  xcb_connection_t *cn;
  xcb_screen_t *scr;

  /* XXX how does this work? */
  g_assert (display_name == NULL);

  cn = xcb_connect (NULL, NULL);
  const xcb_setup_t *setup = xcb_get_setup (cn);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator (setup);
  scr = iter.data;

  if (cn == NULL)
    return NULL;

  dispxcb = g_object_new (gdk_display_xcb_get_type (), NULL);
  dispxcb->default_screen = gdk_screen_xcb_new ((GdkDisplay *) dispxcb, 0, scr);
  dispxcb->cn = cn;

  return (GdkDisplay *) dispxcb;
}

GdkScreen *
gdk_display_get_default_screen (GdkDisplay *display)
{
  GdkDisplayXcb *dispxcb = (GdkDisplayXcb *) display;

  return dispxcb->default_screen;
}
