#ifndef __GSK_PIXEL_SHADER_PRIVATE_H__
#define __GSK_PIXEL_SHADER_PRIVATE_H__

#include "gskpixelshader.h"

#include "gskslnodeprivate.h"

G_BEGIN_DECLS

#define GSK_PIXEL_SHADER_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_TEXTURE, GskPixelShaderClass))
#define GSK_IS_TEXTURE_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_TEXTURE))
#define GSK_PIXEL_SHADER_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_TEXTURE, GskPixelShaderClass))

struct _GskPixelShader
{
  GObject parent_instance;

  GskSlNode *program;

  guint n_textures;
};

struct _GskPixelShaderClass {
  GObjectClass parent_class;
};

G_END_DECLS

#endif /* __GSK_PIXEL_SHADER_PRIVATE_H__ */
