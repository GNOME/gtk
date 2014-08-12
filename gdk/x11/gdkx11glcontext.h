#ifndef __GDK_X11_GL_CONTEXT_H__
#define __GDK_X11_GL_CONTEXT_H__

#if !defined (__GDKX_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkx.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_X11_TYPE_GL_CONTEXT		(gdk_x11_gl_context_get_type ())
#define GDK_X11_GL_CONTEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_X11_TYPE_GL_CONTEXT, GdkX11GLContext))
#define GDK_X11_IS_GL_CONTEXT(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_X11_TYPE_GL_CONTEXT))

typedef struct _GdkX11GLContext		GdkX11GLContext;
typedef struct _GdkX11GLContextClass	GdkX11GLContextClass;

GDK_AVAILABLE_IN_3_14
GType gdk_x11_gl_context_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_14
gboolean        gdk_x11_display_get_glx_version (GdkDisplay *display,
                                                 int        *major,
                                                 int        *minor);

G_END_DECLS

#endif /* __GDK_X11_GL_CONTEXT_H__ */
