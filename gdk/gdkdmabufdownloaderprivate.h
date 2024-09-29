#pragma once

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_DMABUF_DOWNLOADER               (gdk_dmabuf_downloader_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GdkDmabufDownloader, gdk_dmabuf_downloader, GDK, DMABUF_DOWNLOADER, GObject)

struct _GdkDmabufDownloaderInterface
{
  GTypeInterface g_iface;

  void                  (* close)                               (GdkDmabufDownloader            *downloader);
  gboolean              (* download)                            (GdkDmabufDownloader            *downloader,
                                                                 GdkDmabufTexture               *texture,
                                                                 GdkMemoryFormat                 format,
                                                                 GdkColorState                  *color_state,
                                                                 guchar                         *data,
                                                                 gsize                           stride);
};

void                    gdk_dmabuf_downloader_close             (GdkDmabufDownloader            *self);
gboolean                gdk_dmabuf_downloader_download          (GdkDmabufDownloader            *downloader,
                                                                 GdkDmabufTexture               *texture,
                                                                 GdkMemoryFormat                 format,
                                                                 GdkColorState                  *color_state,
                                                                 guchar                         *data,
                                                                 gsize                           stride);


G_END_DECLS

