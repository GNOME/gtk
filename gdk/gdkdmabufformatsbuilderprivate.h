#pragma once

#include "gdkdmabufformats.h"

typedef struct GdkDmabufFormatsBuilder GdkDmabufFormatsBuilder;

GdkDmabufFormatsBuilder *gdk_dmabuf_formats_builder_new                        (void);
GdkDmabufFormats *       gdk_dmabuf_formats_builder_free_to_formats            (GdkDmabufFormatsBuilder *self);

void                     gdk_dmabuf_formats_builder_add_format                 (GdkDmabufFormatsBuilder *self,
                                                                                guint32                  fourcc,
                                                                                guint64                  modifier);

GdkDmabufFormats *       gdk_dmabuf_formats_builder_free_to_formats_for_device (GdkDmabufFormatsBuilder *self,
                                                                                guint64                  device);
void                     gdk_dmabuf_formats_builder_add_format_for_device      (GdkDmabufFormatsBuilder *self,
                                                                                guint32                  fourcc,
                                                                                guint32                  flags,
                                                                                guint64                  modifier,
                                                                                guint64                  device);

void                     gdk_dmabuf_formats_builder_next_priority              (GdkDmabufFormatsBuilder *self);
void                     gdk_dmabuf_formats_builder_add_formats                (GdkDmabufFormatsBuilder *self,
                                                                                GdkDmabufFormats        *formats);
