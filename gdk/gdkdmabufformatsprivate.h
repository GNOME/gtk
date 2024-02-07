#pragma once

#include "gdkdmabufformats.h"

typedef struct _GdkDmabufFormat GdkDmabufFormat;
struct _GdkDmabufFormat
{
  guint32 fourcc;
  guint32 flags;
  guint64 modifier;
  guint64 device;
  gsize next_priority;
};

struct _GdkDmabufFormats
{
  int ref_count;

  gsize n_formats;
  GdkDmabufFormat *formats;
  guint64 device;
};

GdkDmabufFormats *      gdk_dmabuf_formats_new                  (GdkDmabufFormat  *formats,
                                                                 gsize             n_formats,
                                                                 guint64           device);

const GdkDmabufFormat * gdk_dmabuf_formats_peek_formats         (GdkDmabufFormats *self);
