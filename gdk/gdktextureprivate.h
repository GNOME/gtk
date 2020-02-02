#ifndef __GDK_TEXTURE_PRIVATE_H__
#define __GDK_TEXTURE_PRIVATE_H__

#include "gdktexture.h"

G_BEGIN_DECLS

struct _GdkTexture
{
  GObject parent_instance;

  int width;
  int height;

  gpointer render_key;
  gpointer render_data;
  GDestroyNotify render_notify;
};

struct _GdkTextureClass {
  GObjectClass parent_class;

  void                  (* download)                    (GdkTexture             *texture,
                                                         const GdkRectangle     *area,
                                                         guchar                 *data,
                                                         gsize                   stride);
  cairo_surface_t *     (* download_surface)            (GdkTexture             *texture);
};

gpointer                gdk_texture_new                 (const GdkTextureClass  *klass,
                                                         int                     width,
                                                         int                     height);
GdkTexture *            gdk_texture_new_for_surface     (cairo_surface_t        *surface);
cairo_surface_t *       gdk_texture_download_surface    (GdkTexture             *texture);
void                    gdk_texture_download_area       (GdkTexture             *texture,
                                                         const GdkRectangle     *area,
                                                         guchar                 *data,
                                                         gsize                   stride);

gboolean                gdk_texture_set_render_data     (GdkTexture             *self,
                                                         gpointer                key,
                                                         gpointer                data,
                                                         GDestroyNotify          notify);
void                    gdk_texture_clear_render_data   (GdkTexture             *self);
gpointer                gdk_texture_get_render_data     (GdkTexture             *self,
                                                         gpointer                key);

G_END_DECLS

#endif /* __GDK_TEXTURE_PRIVATE_H__ */
