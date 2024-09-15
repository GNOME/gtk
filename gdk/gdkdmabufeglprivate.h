#pragma once

#include "gdkdisplayprivate.h"
#if defined(HAVE_DMABUF) && defined (HAVE_EGL)

#include "gdkdmabufprivate.h"

#include <epoxy/egl.h>

EGLImage                    gdk_dmabuf_egl_create_image         (GdkDisplay                     *display,
                                                                 int                             width,
                                                                 int                             height,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 int                             target);

#endif  /* HAVE_DMABUF && HAVE_EGL */

void                        gdk_dmabuf_egl_init                 (GdkDisplay                     *display);
