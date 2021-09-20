#ifndef __GDK_TEXTURE_PRIVATE_H__
#define __GDK_TEXTURE_PRIVATE_H__

#include "gdktexture.h"

G_BEGIN_DECLS

#define GDK_TEXTURE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_TEXTURE, GdkTextureClass))
#define GDK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_TEXTURE))
#define GDK_TEXTURE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_TEXTURE, GdkTextureClass))

struct _GdkTexture
{
  GObject parent_instance;

  int width;
  int height;
  GdkColorProfile *color_profile;

  gpointer render_key;
  gpointer render_data;
  GDestroyNotify render_notify;
};

struct _GdkTextureClass {
  GObjectClass parent_class;

  /* mandatory: Download into a GdkMemoryTexture */
  GdkTexture *          (* download_texture)            (GdkTexture             *texture);
  /* optional */
  void                  (* download)                    (GdkTexture             *texture,
                                                         guchar                 *data,
                                                         gsize                   stride);
  void                  (* download_float)              (GdkTexture             *texture,
                                                         float                  *data,
                                                         gsize                   stride);
};

gboolean                gdk_texture_can_load            (GBytes                 *bytes);

GdkTexture *            gdk_texture_new_for_surface     (cairo_surface_t        *surface);
cairo_surface_t *       gdk_texture_download_surface    (GdkTexture             *texture);
/* NB: GdkMemoryTexture */
GdkTexture *            gdk_texture_download_texture    (GdkTexture             *texture);

gboolean                gdk_texture_set_render_data     (GdkTexture             *self,
                                                         gpointer                key,
                                                         gpointer                data,
                                                         GDestroyNotify          notify);
void                    gdk_texture_clear_render_data   (GdkTexture             *self);
gpointer                gdk_texture_get_render_data     (GdkTexture             *self,
                                                         gpointer                key);

G_END_DECLS

#endif /* __GDK_TEXTURE_PRIVATE_H__ */
