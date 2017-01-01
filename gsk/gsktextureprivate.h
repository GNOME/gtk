#ifndef __GSK_TEXTURE_PRIVATE_H__
#define __GSK_TEXTURE_PRIVATE_H__

#include "gsktexture.h"

G_BEGIN_DECLS

#define GSK_TEXTURE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_TEXTURE, GskTextureClass))
#define GSK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_TEXTURE))
#define GSK_TEXTURE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_TEXTURE, GskTextureClass))

struct _GskTexture
{
  GObject parent_instance;

  int width;
  int height;

  gpointer render_key;
  gpointer render_data;
  GDestroyNotify render_notify;
};

struct _GskTextureClass {
  GObjectClass parent_class;

  void                  (* download)                    (GskTexture             *texture,
                                                         guchar                 *data,
                                                         gsize                   stride);
  cairo_surface_t *     (* download_surface)            (GskTexture             *texture);
};

gpointer                gsk_texture_new                 (const GskTextureClass  *klass,
                                                         int                     width,
                                                         int                     height);
GskTexture *            gsk_texture_new_for_surface     (cairo_surface_t        *surface);
cairo_surface_t *       gsk_texture_download_surface    (GskTexture             *texture);

gboolean                gsk_texture_set_render_data     (GskTexture             *self,
                                                         gpointer                key,
                                                         gpointer                data,
                                                         GDestroyNotify          notify);
void                    gsk_texture_clear_render_data   (GskTexture             *self);
gpointer                gsk_texture_get_render_data     (GskTexture             *self,
                                                         gpointer                key);

G_END_DECLS

#endif /* __GSK_TEXTURE_PRIVATE_H__ */
