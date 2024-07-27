#pragma once

#if defined(HAVE_DMABUF) && defined (HAVE_EGL)

#include "gdkdmabufprivate.h"
#include "gdkdmabufdownloaderprivate.h"

#include <epoxy/egl.h>

GdkDmabufDownloader *       gdk_dmabuf_get_egl_downloader       (GdkDisplay                     *display,
                                                                 GdkDmabufFormatsBuilder        *builder);
EGLImage                    gdk_dmabuf_egl_create_image         (GdkDisplay                     *display,
                                                                 int                             width,
                                                                 int                             height,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 int                             color_state_hint,
                                                                 int                             range_hint,
                                                                 int                             target);

void                        gdk_dmabuf_get_egl_yuv_hints        (const GdkDmabuf                *dmabuf,
                                                                 GdkColorState                  *color_state,
                                                                 int                            *color_space_hint,
                                                                 int                            *range_hint);

#endif  /* HAVE_DMABUF && HAVE_EGL */
