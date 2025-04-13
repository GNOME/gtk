#pragma once

#include <gdk/gdktypes.h>

#include "gdkmemorylayoutprivate.h"

#include <directx/d3d12.h>

G_BEGIN_DECLS

void                    gdk_d3d12_resource_get_layout                   (ID3D12Resource                         *resource,
                                                                         GdkMemoryFormat                         format,
                                                                         GdkMemoryLayout                        *out_layout,
                                                                         D3D12_PLACED_SUBRESOURCE_FOOTPRINT      out_footprints[GDK_MEMORY_MAX_PLANES]);

ID3D12Resource *        gdk_d3d12_resource_new_from_bytes               (const guchar                           *data,
                                                                         const GdkMemoryLayout                  *layout,
                                                                         GError                                **error);

void                    gdk_d3d12_command_queue_wait_sync               (ID3D12CommandQueue                     *queue);
void                    gdk_d3d12_fence_wait_sync                       (ID3D12Fence                            *fence,
                                                                         guint64                                 fence_value);

G_END_DECLS
