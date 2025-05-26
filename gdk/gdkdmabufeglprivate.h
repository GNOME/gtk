#pragma once

#include "gdkdisplayprivate.h"
#if defined(HAVE_DMABUF) && defined (HAVE_EGL)

#include "gdkdmabufprivate.h"

#include <epoxy/egl.h>

guint                       gdk_dmabuf_egl_import_dmabuf        (GdkGLContext                   *context,
                                                                 int                             width,
                                                                 int                             height,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 EGLint                          color_space_hint,
                                                                 EGLint                          range_hint,
                                                                 gboolean                       *external);
gsize                       gdk_dmabuf_egl_import_dmabuf_multiplane
                                                                (GdkGLContext                   *context,
                                                                 int                             width,
                                                                 int                             height,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 guint                           out_tex_id[3]);

#endif  /* HAVE_DMABUF && HAVE_EGL */

void                        gdk_dmabuf_egl_init                 (GdkDisplay                     *display);
