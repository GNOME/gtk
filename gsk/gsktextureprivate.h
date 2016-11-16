#ifndef __GSK_TEXTURE_PRIVATE_H__
#define __GSK_TEXTURE_PRIVATE_H__

#include "gsktexture.h"

G_BEGIN_DECLS

#define GSK_TEXTURE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_TEXTURE, GskTextureClass))
#define GSK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_TEXTURE))
#define GSK_TEXTURE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_TEXTURE, GskTextureClass))

typedef struct _GskTextureClass GskTextureClass;

struct _GskTexture
{
  const GskTextureClass *klass;

  volatile int ref_count;

  int width;
  int height;
};

struct _GskTextureClass {
  const char *name;
  gsize size;

  void                  (* finalize)                    (GskTexture             *texture);
  cairo_surface_t *     (* download)                    (GskTexture             *texture);
};

gpointer                gsk_texture_new                 (const GskTextureClass  *klass,
                                                         int                     width,
                                                         int                     height);
GskTexture *            gsk_texture_new_for_surface     (cairo_surface_t        *surface);
cairo_surface_t *       gsk_texture_download            (GskTexture             *texture);

G_END_DECLS

#endif /* __GSK_TEXTURE_PRIVATE_H__ */
