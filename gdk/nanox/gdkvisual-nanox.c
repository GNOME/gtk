
#include "gdkvisual.h"
#include "gdkprivate-nanox.h"
#include <glib.h>

static GdkVisual system_visual;

void
gdk_visual_init (void)
{
  system_visual.type = GDK_VISUAL_TRUE_COLOR;
  system_visual.depth = 24;
  system_visual.bits_per_rgb = 8;
}

GdkVisual*
gdk_visual_ref (GdkVisual *visual)
{
  return visual;
}

void
gdk_visual_unref (GdkVisual *visual)
{
  return;
}

gint
gdk_visual_get_best_depth (void)
{
  return 24;
}

GdkVisualType
gdk_visual_get_best_type (void)
{
  return GDK_VISUAL_TRUE_COLOR;
}

GdkVisual*
gdk_visual_get_system (void)
{
  return ((GdkVisual*) &system_visual);
}

GdkVisual*
gdk_visual_get_best (void)
{
  return ((GdkVisual*) &system_visual);
}

GdkVisual*
gdk_visual_get_best_with_type (GdkVisualType visual_type)
{
  if (visual_type == GDK_VISUAL_TRUE_COLOR)
    return &system_visual;
  return NULL;
}

GdkVisual*
gdk_visual_get_best_with_both (gint          depth,
			       GdkVisualType visual_type)
{
  if (visual_type == GDK_VISUAL_TRUE_COLOR && depth == 24)
    return &system_visual;
  return NULL;
}

void
gdk_query_depths  (gint **depths,
		   gint  *count)
{
  if (count)
    *count = 1;
  if (depths)
    *depths[0] = 24;
}

void
gdk_query_visual_types (GdkVisualType **visual_types,
			gint           *count)
{
  if (count)
    *count = 1;
  if (visual_types)
    *visual_types[0] = GDK_VISUAL_TRUE_COLOR;
}

GList*
gdk_list_visuals (void)
{
  return g_list_append(NULL, &system_visual);
}


