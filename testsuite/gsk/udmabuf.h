#pragma once

#include <glib.h>

typedef struct
{
  int mem_fd;
  int dmabuf_fd;
  size_t size;
  gpointer data;
} UDmabuf;

void     udmabuf_free       (gpointer   data);

gboolean udmabuf_initialize (GError   **error);

UDmabuf *udmabuf_allocate   (size_t     size,
                             GError   **error);
