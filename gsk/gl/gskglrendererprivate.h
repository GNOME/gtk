#ifndef __GSK_GL_RENDERER_PRIVATE_H__
#define __GSK_GL_RENDERER_PRIVATE_H__

#include <gsk/gskrenderer.h>

G_BEGIN_DECLS

#define GSK_TYPE_GL_RENDERER (gsk_gl_renderer_get_type ())

#define GSK_GL_RENDERER(obj)                    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_GL_RENDERER, GskGLRenderer))
#define GSK_IS_GL_RENDERER(obj)                 (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_GL_RENDERER))
#define GSK_GL_RENDERER_CLASS(klass)            (G_TYPE_CHECK_CLASS_CAST ((klass), GSK_TYPE_GL_RENDERER, GskGLRendererClass))
#define GSK_IS_GL_RENDERER_CLASS(klass)         (G_TYPE_CHECK_CLASS_TYPE ((klass), GSK_TYPE_GL_RENDERER))
#define GSK_GL_RENDERER_GET_CLASS(obj)          (G_TYPE_INSTANCE_GET_CLASS ((obj), GSK_TYPE_GL_RENDERER, GskGLRendererClass))

typedef struct _GskGLRenderer                   GskGLRenderer;
typedef struct _GskGLRendererClass              GskGLRendererClass;

GType gsk_gl_renderer_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GSK_GL_RENDERER_PRIVATE_H__ */
