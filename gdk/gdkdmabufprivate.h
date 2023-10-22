#pragma once

#include "gdkdmabufformatsbuilderprivate.h"

#define GDK_DMABUF_MAX_PLANES 4

typedef struct _GdkDmabuf GdkDmabuf;
typedef struct _GdkDmabufDownloader GdkDmabufDownloader;

struct _GdkDmabuf
{
  guint32 fourcc;
  guint64 modifier;
  unsigned int n_planes;
  struct {
    int fd;
    unsigned int stride;
    unsigned int offset;
  } planes[GDK_DMABUF_MAX_PLANES];
};

struct _GdkDmabufDownloader
{
  const char *name;
  void                  (* add_formats)                         (const GdkDmabufDownloader      *downloader,
                                                                 GdkDisplay                     *display,
                                                                 GdkDmabufFormatsBuilder        *builder);

  gboolean              (* supports)                            (const GdkDmabufDownloader      *downloader,
                                                                 GdkDisplay                     *display,
                                                                 const GdkDmabuf                *dmabuf,
                                                                 gboolean                        premultiplied,
                                                                 GdkMemoryFormat                *out_format,
                                                                 GError                        **error);
  void                  (* download)                            (const GdkDmabufDownloader      *downloader,
                                                                 GdkTexture                     *texture,
                                                                 GdkMemoryFormat                 format,
                                                                 guchar                         *data,
                                                                 gsize                           stride);
};

#ifdef HAVE_LINUX_DMA_BUF_H
const GdkDmabufDownloader *
                        gdk_dmabuf_get_direct_downloader        (void) G_GNUC_CONST;

gboolean                gdk_dmabuf_sanitize                     (GdkDmabuf                      *dest,
                                                                 gsize                           width,
                                                                 gsize                           height,
                                                                 const GdkDmabuf                *src,
                                                                 GError                        **error);

const char *            gdk_dmabuf_fourcc_print   (char    *str,
                                                   gsize    size,
                                                   guint32  fourcc);
const char *            gdk_dmabuf_modifier_print (char    *str,
                                                   gsize    size,
                                                   guint64  modifier);
const char *            gdk_dmabuf_format_print   (char    *str,
                                                   gsize    size,
                                                   guint32  fourcc,
                                                   guint64  modifier);

#endif
