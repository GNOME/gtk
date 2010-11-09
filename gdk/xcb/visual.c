#include "visual.h"
#include "gdk.h"

struct _GdkVisualClass {
  GObjectClass parent_class;
};

struct _GdkVisualPrivate {
  GdkScreen *screen;
};

G_DEFINE_TYPE (GdkVisual, gdk_visual, G_TYPE_OBJECT)

static void
gdk_visual_init (GdkVisual *visual)
{
  visual->priv = G_TYPE_INSTANCE_GET_PRIVATE (visual,
                                              GDK_TYPE_VISUAL,
                                              GdkVisualPrivate);
}

static void
gdk_visual_class_init (GdkVisualClass *class)
{
  g_type_class_add_private (class, sizeof (GdkVisualPrivate));
}

GdkScreen *
gdk_visual_get_screen (GdkVisual *visual)
{
  return visual->priv->screen;
}

GdkVisual *
gdk_visual_xcb_new (GdkScreen *screen)
{
  GdkVisual *visual;

  visual = g_object_new (gdk_visual_get_type (), NULL);
  visual->priv->screen = screen;

  return visual;
}
