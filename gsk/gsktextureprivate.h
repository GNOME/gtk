#ifndef __GSK_TEXTURE_PRIVATE_H__
#define __GSK_TEXTURE_PRIVATE_H__

#include "gsktexture.h"

G_BEGIN_DECLS

#define GSK_TEXTURE_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_TEXTURE, GskTextureClass))
#define GSK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_TEXTURE))
#define GSK_TEXTURE_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_TEXTURE, GskTextureClass))

struct _GskTexture
{
  volatile int ref_count;

  GskRenderer *renderer;
  int width;
  int height;
};

#define gsk_texture_new(type,renderer,width,height) \
  (type *) gsk_texture_alloc(sizeof (type),(renderer),(width),(height))
GskTexture *gsk_texture_alloc (gsize           size,
                               GskRenderer    *renderer,
                               int             width,
                               int             height);

G_END_DECLS

#endif /* __GSK_TEXTURE_PRIVATE_H__ */
