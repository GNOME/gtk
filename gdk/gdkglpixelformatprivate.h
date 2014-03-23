#ifndef __GDK_GL_PIXEL_FORMAT_PRIVATE_H__
#define __GDK_GL_PIXEL_FORMAT_PRIVATE_H__

#include "gdkglpixelformat.h"

G_BEGIN_DECLS

#define GDK_GL_PIXEL_FORMAT_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_GL_PIXEL_FORMAT, GdkGLPixelFormatClass))
#define GDK_IS_GL_PIXEL_FORMAT_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_GL_PIXEL_FORMAT))
#define GDK_GL_PIXEL_FORMAT_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_GL_PIXEL_FORMAT, GdkGLPixelFormatClass))

struct _GdkGLPixelFormat
{
  GObject parent_instance;

  gboolean is_validated;
  gboolean is_valid;

  GdkDisplay *display;

  gboolean double_buffer;
  gboolean multi_sample;

  int aux_buffers;
  int color_size;
  int alpha_size;
  int depth_size;
  int stencil_size;
  int accum_size;
  int sample_buffers;
  int samples;

  GdkGLPixelFormatProfile profile;
};

struct _GdkGLPixelFormatClass
{
  GObjectClass parent_class;
};

G_END_DECLS

#endif /* __GDK_GL_PIXEL_FORMAT_PRIVATE_H__ */
