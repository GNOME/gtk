/* gdkd3d12utils.c
 *
 * Copyright 2025  Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkd3d12utilsprivate.h"

#include "gdkmemoryformatprivate.h"
#include "gdkprivate-win32.h"

/*<private>
 * gdk_d3d12_resource_get_layout:
 * @resource: The resource to query
 * @format: The format of the resource
 * @out_layout: (out): The layout to be used for the given resource
 * @out_footprints: (out): The footprints to be used for the given resource
 *
 * Queries the layout and footprints for a buffer resource to be used when
 * copying data to or from @resource.
 */
void
gdk_d3d12_resource_get_layout (ID3D12Resource                     *resource,
                               GdkMemoryFormat                     format,
                               GdkMemoryLayout                    *out_layout,
                               D3D12_PLACED_SUBRESOURCE_FOOTPRINT  out_footprints[GDK_MEMORY_MAX_PLANES])
{
  ID3D12Device *device;
  D3D12_RESOURCE_DESC resource_desc;
  gsize p, n_planes;
  UINT64 buffer_size;

  hr_warn (ID3D12Resource_GetDevice (resource,
                                     &IID_ID3D12Device,
                                     (void **) &device));

  ID3D12Resource_GetDesc (resource, &resource_desc);
  n_planes = gdk_memory_format_get_n_planes (format);
  ID3D12Device_GetCopyableFootprints (device,
                                      &resource_desc,
                                      0, n_planes,
                                      0,
                                      out_footprints,
                                      NULL,
                                      NULL,
                                      &buffer_size);

  out_layout->format = format;
  out_layout->width = resource_desc.Width;
  out_layout->height = resource_desc.Height;
  out_layout->size = buffer_size;
  for (p = 0; p < n_planes; p++)
    {
      out_layout->planes[p].offset = out_footprints[p].Offset;
      out_layout->planes[p].stride = out_footprints[p].Footprint.RowPitch;
    }
}

ID3D12Resource *
gdk_d3d12_resource_new_from_bytes (const guchar           *data,
                                   const GdkMemoryLayout  *layout,
                                   GError                **error)
{
  ID3D12Device *device;
  HRESULT hr;
  ID3D12CommandAllocator *allocator = NULL;
  ID3D12GraphicsCommandList *commands = NULL;
  ID3D12CommandQueue *queue = NULL;
  ID3D12Resource *buffer = NULL, *resource;
  GdkMemoryLayout buffer_layout;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[GDK_MEMORY_MAX_PLANES];
  DXGI_FORMAT format;
  void *buffer_data;
  gsize p;

  hr = D3D12CreateDevice (NULL, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **) &device);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create device"))
    return NULL;

  format = gdk_memory_format_get_dxgi_format (layout->format, NULL);
  if (format == DXGI_FORMAT_UNKNOWN)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
                   "Memory format %s is not supported",
                   gdk_memory_format_get_name (layout->format));
      return NULL;
    }

  hr = ID3D12Device_CreateCommittedResource (device,
                                             (&(D3D12_HEAP_PROPERTIES) {
                                                 .Type = D3D12_HEAP_TYPE_DEFAULT,
                                                 .CreationNodeMask = 1,
                                                 .VisibleNodeMask = 1,
                                             }),
                                             D3D12_HEAP_FLAG_SHARED,
                                             (&(D3D12_RESOURCE_DESC) {
                                                 .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                                 .Width = layout->width,
                                                 .Height = layout->height,
                                                 .DepthOrArraySize = 1,
                                                 .MipLevels = 1,
                                                 .Format = format,
                                                 .SampleDesc = {
                                                     .Count = 1,
                                                     .Quality = 0,
                                                 },
                                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                                 .Flags = D3D12_RESOURCE_FLAG_NONE,
                                             }),
                                             D3D12_RESOURCE_STATE_COMMON,
                                             NULL,
                                             &IID_ID3D12Resource,
                                             (void **) &resource);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create resource"))
    return NULL;

  gdk_d3d12_resource_get_layout (resource,
                                 layout->format,
                                 &buffer_layout,
                                 footprints);

  hr = ID3D12Device_CreateCommittedResource (device,
                                             (&(D3D12_HEAP_PROPERTIES) {
                                                 .Type = D3D12_HEAP_TYPE_UPLOAD,
                                                 .CreationNodeMask = 1,
                                                 .VisibleNodeMask = 1,
                                             }),
                                             D3D12_HEAP_FLAG_NONE,
                                             (&(D3D12_RESOURCE_DESC) {
                                                 .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                                 .Width = buffer_layout.size,
                                                 .Height = 1,
                                                 .DepthOrArraySize = 1,
                                                 .MipLevels = 1,
                                                 .SampleDesc = {
                                                     .Count = 1,
                                                     .Quality = 0,
                                                 },
                                                 .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
                                             }),
                                             D3D12_RESOURCE_STATE_GENERIC_READ,
                                             NULL,
                                             &IID_ID3D12Resource,
                                             (void **) &buffer);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create upload buffer"))
    goto out;

  ID3D12Resource_Map (buffer, 0, (&(D3D12_RANGE) { 0, buffer_layout.size }), &buffer_data );

  gdk_memory_copy (buffer_data,
                   &buffer_layout,
                   data,
                   layout);

  ID3D12Resource_Unmap (buffer, 0, (&(D3D12_RANGE) { 0, buffer_layout.size }));

  hr = ID3D12Device_CreateCommandAllocator (device,
                                            D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            &IID_ID3D12CommandAllocator,
                                            (void **) &allocator);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create command allocator"))
    goto out;

  hr = ID3D12Device_CreateCommandList (device,
                                       0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator,
                                       NULL,
                                       &IID_ID3D12GraphicsCommandList,
                                       (void **) &commands);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create command list"))
    goto out;

  for (p = 0; p < gdk_memory_format_get_n_planes (layout->format); p++)
    {
      ID3D12GraphicsCommandList_CopyTextureRegion (commands,
                                                  (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                      .pResource = resource,
                                                      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                                      .SubresourceIndex = p,
                                                  }),
                                                  0, 0, 0,
                                                  (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                      .pResource = buffer,
                                                      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                                                      .PlacedFootprint = footprints[p],
                                                  }),
                                                  NULL);
    }
  hr = ID3D12GraphicsCommandList_Close (commands);
  if (!gdk_win32_check_hresult (hr, error, "Failed to close command list"))
    goto out;

  hr = ID3D12Device_CreateCommandQueue (device,
                                        (&(D3D12_COMMAND_QUEUE_DESC) {
                                            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                        }),
                                        &IID_ID3D12CommandQueue,
                                        (void **) &queue);
  if (!gdk_win32_check_hresult (hr, error, "Failed to create command queue"))
    goto out;

  ID3D12CommandQueue_ExecuteCommandLists (queue, 1, (ID3D12CommandList **) &commands);

  gdk_d3d12_command_queue_wait_sync (queue);

out:
  if (!SUCCEEDED (hr))
    {
      gdk_win32_com_clear (&resource);
      resource = NULL;
    }
  gdk_win32_com_clear (&buffer);
  gdk_win32_com_clear (&commands);
  gdk_win32_com_clear (&allocator);
  gdk_win32_com_clear (&queue);
  gdk_win32_com_clear (&device);

  return resource;
}

/*<private>
 * @queue: The queue to wait on
 *
 * Busy-waits for queue to have all commands executed. This is
 * done by adding a fence and busy-waiting for it to be signaled.
 * Same as glWaitSync() or gdk_d3d12_fence_wait_sync(), but for
 * D3D12 queues.
 */
void
gdk_d3d12_command_queue_wait_sync (ID3D12CommandQueue *queue)
{
  ID3D12Device *device;
  ID3D12Fence *fence;

  hr_warn (ID3D12Resource_GetDevice (queue,
                                     &IID_ID3D12Device,
                                     (void **) &device));
  hr_warn (ID3D12Device_CreateFence (device,
                                     0,
                                     D3D12_FENCE_FLAG_NONE,
                                     &IID_ID3D12Fence,
                                     (void **) &fence));

#define FENCE_SIGNAL 1
  hr_warn (ID3D12CommandQueue_Signal (queue, fence, FENCE_SIGNAL));
  gdk_d3d12_fence_wait_sync (fence, FENCE_SIGNAL);
#undef FENCE_SIGNAL

  ID3D12Fence_Release (fence);
  ID3D12Device_Release (device);
}

/*<private>
 * gdk_d3d12_fence_wait_sync:
 * @fence: fence to wait on
 * @fence_value: The value to wait for
 *
 * Busy-waits for fence to be signaled. Same as glWaitSync(), but for
 * D3D12 fences.
 */
void
gdk_d3d12_fence_wait_sync (ID3D12Fence *fence,
                           guint64      fence_value)
{
  HANDLE event;

  event = CreateEvent (NULL, FALSE, FALSE, NULL);
  hr_warn (ID3D12Fence_SetEventOnCompletion (fence, fence_value, event));
  WaitForSingleObject (event, INFINITE);
  CloseHandle (event);
}
