#include "gdkwindowimpl.h"
#include "gdkinternals.h"

typedef struct
{
  GObject parent_instance;
} GdkWindowImplXcb;

typedef struct
{
  GObjectClass parent_class;
} GdkWindowImplXcbClass;

static void gdk_window_impl_iface_init (GdkWindowImplIface *iface);
static GType gdk_window_impl_xcb_get_type (void);
G_DEFINE_TYPE_WITH_CODE (GdkWindowImplXcb,
                         gdk_window_impl_xcb,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_WINDOW_IMPL,
                                                gdk_window_impl_iface_init))

static void
gdk_window_impl_xcb_init (GdkWindowImplXcb *implxcb)
{
}

static void
gdk_window_impl_xcb_class_init (GdkWindowImplXcbClass *class)
{
}

static void
gdk_window_impl_iface_init (GdkWindowImplIface *iface)
{
}

void
_gdk_window_impl_new (GdkWindow *window,
                      GdkWindow *real_parent,
                      GdkScreen *screen,
                      GdkEventMask event_mask,
                      GdkWindowAttr *attributes,
                      gint attributes_mask)
{
  GdkWindowObject *private = (GdkWindowObject *) window;

  private->impl = g_object_new (gdk_window_impl_xcb_get_type (), NULL);
}
