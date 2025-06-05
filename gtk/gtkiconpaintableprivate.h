#pragma once

#include "gtkicontheme.h"
#include "gtkiconpaintable.h"
#include "gsk/gsktypes.h"

typedef struct {
  char **icon_names;
  int size;
  int scale;
  GtkIconLookupFlags flags;
} IconKey;

struct _GtkIconPaintable
{
  GObject parent_instance;

  /* Information about the source
   */
  IconKey key;
  GtkIconTheme *in_cache; /* Protected by icon_cache lock */

  char *icon_name;
  char *filename;
  GLoadableIcon *loadable;

  /* Parameters influencing the scaled icon
   */
  int desired_size;
  int desired_scale;
  guint is_svg          : 1;
  guint is_resource     : 1;
  guint is_symbolic     : 1;
  guint only_fg         : 1;
  guint single_path     : 1;

  /* Debug flags for testing svg->node conversion */
  guint allow_node     : 1;
  guint allow_recolor  : 1;
  guint allow_mask     : 1;

  /* Cached information if we go ahead and try to load the icon.
   *
   * All access to these are protected by the texture_lock. Everything
   * above is immutable after construction and can be used without
   * locks.
   */
  GMutex texture_lock;

  GskRenderNode *node;
  double width;
  double height;
};

GtkIconPaintable *gtk_icon_paintable_new_for_texture (GdkTexture *texture,
                                                      int         desired_size,
                                                      int         desired_scale);
GtkIconPaintable *gtk_icon_paintable_new_for_path     (const char *path,
                                                       gboolean    is_resource,
                                                       int         desired_size,
                                                       int         desired_scale);
GtkIconPaintable *gtk_icon_paintable_new_for_loadable (GLoadableIcon *loadable,
                                                       int            desired_size,
                                                       int            desired_scale);

void gtk_icon_paintable_set_debug (GtkIconPaintable *icon,
                                   gboolean          allow_node,
                                   gboolean          allow_recolor,
                                   gboolean          allow_mask);
void gtk_icon_paintable_set_icon_name (GtkIconPaintable *icon,
                                       const char       *name);

void gtk_icon_paintable_load_in_thread (GtkIconPaintable *self);

