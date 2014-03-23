#ifndef __GDK_X11_GL_CONTEXT__
#define __GDK_X11_GL_CONTEXT__

#include <X11/X.h>
#include <X11/Xlib.h>

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include "gdkglcontextprivate.h"
#include "gdkglpixelformatprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglpixelformat.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"

G_BEGIN_DECLS

struct _GdkX11GLContext
{
  GdkGLContext parent_instance;

  GLXContext glx_context;
  GLXFBConfig glx_config;

  GLXDrawable current_drawable;

  Window dummy_drawable;
  GLXWindow dummy_glx_drawable;

  guint is_direct : 1;
};

struct _GdkX11GLContextClass
{
  GdkGLContextClass parent_class;
};

gboolean        gdk_x11_display_init_gl                         (GdkDisplay        *display);
gboolean        gdk_x11_display_validate_gl_pixel_format        (GdkDisplay        *display,
                                                                 GdkGLPixelFormat  *format,
                                                                 GError           **error);
GdkGLContext *  gdk_x11_display_create_gl_context               (GdkDisplay        *display,
                                                                 GdkGLPixelFormat  *format,
                                                                 GdkGLContext      *share);
void            gdk_x11_display_destroy_gl_context              (GdkDisplay        *display,
                                                                 GdkGLContext      *context);
gboolean        gdk_x11_display_make_gl_context_current         (GdkDisplay        *display,
                                                                 GdkGLContext      *context,
                                                                 GdkWindow         *window);

G_END_DECLS

#endif /* __GDK_X11_GL_CONTEXT__ */
