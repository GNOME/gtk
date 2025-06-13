/* gdkd3d12texture.c
 *
 * Copyright 2024  Red Hat, Inc.
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

#include "gdkd3d12textureprivate.h"

#include "gdkcolorstateprivate.h"
#include "gdkd3d12texturebuilder.h"
#include "gdkd3d12utilsprivate.h"
#include "gdkglcontextprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gdkprivate-win32.h"
#include "gdktextureprivate.h"

#include <epoxy/gl.h>

/**
 * GdkD3D12Texture:
 *
 * A `GdkTexture` representing a [ID3D12Resource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nn-d3d12-id3d12resource).
 *
 * To create a `GdkD3D12Texture`, use the auxiliary
 * [class@Gdk.Win32.D3d12TextureBuilder] object.
 *
 * D3D12 textures can only be created on Windows.
 *
 * Since: 4.20
 */

struct _GdkD3D12Texture
{
  GdkTexture parent_instance;

  ID3D12Resource *resource;
  HANDLE resource_handle;
  ID3D12Fence *fence;
  HANDLE fence_handle;
  guint64 fence_wait;

  GDestroyNotify destroy;
  gpointer data;
};

struct _GdkD3D12TextureClass
{
  GdkTextureClass parent_class;
};

/**
 * gdk_d3d12_error_quark:
 *
 * Registers an error quark for [class@Gdk.Win32.D3d12Texture] errors.
 *
 * Returns: the error quark
 **/
G_DEFINE_QUARK (gdk-d3d12-error-quark, gdk_d3d12_error)

G_DEFINE_TYPE (GdkD3D12Texture, gdk_d3d12_texture, GDK_TYPE_TEXTURE)

static void
gdk_d3d12_texture_dispose (GObject *object)
{
  GdkD3D12Texture *self = GDK_D3D12_TEXTURE (object);

  if (self->fence_handle)
    {
      if (G_UNLIKELY (!CloseHandle (self->fence_handle)))
        WIN32_API_FAILED ("CloseHandle (fence_handle)");
      self->fence_handle = NULL;
    }
  if (self->resource_handle)
    {
      if (G_UNLIKELY (!CloseHandle (self->resource_handle)))
        WIN32_API_FAILED ("CloseHandle (resource_handle)");
      self->resource_handle = NULL;
    }

  gdk_win32_com_clear (&self->fence);
  gdk_win32_com_clear (&self->resource);

    if (self->destroy)
    {
      self->destroy (self->data);
      self->destroy = NULL;
    }

  G_OBJECT_CLASS (gdk_d3d12_texture_parent_class)->dispose (object);
}

static gboolean
supports_nonzero (ID3D12Device *device)
{
  D3D12_FEATURE_DATA_D3D12_OPTIONS7 options;
  HRESULT hr;

  hr = ID3D12Device_CheckFeatureSupport (device,
                                         D3D12_FEATURE_D3D12_OPTIONS7,
                                         &options,
                                         sizeof (options));
  return SUCCEEDED (hr);
}

static void
gdk_d3d12_texture_download (GdkTexture            *texture,
                            guchar                *data,
                            const GdkMemoryLayout *layout,
                            GdkColorState         *color_state)
{
  GdkD3D12Texture *self = GDK_D3D12_TEXTURE (texture);
  ID3D12Device *device;
  ID3D12CommandAllocator *allocator;
  ID3D12GraphicsCommandList *commands;
  ID3D12CommandQueue *queue;
  ID3D12Resource *buffer;
  GdkMemoryLayout buffer_layout;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[GDK_MEMORY_MAX_PLANES];
  void *buffer_data;
  gsize p, n_planes;

  hr_warn (ID3D12Resource_GetDevice (self->resource,
                                     &IID_ID3D12Device,
                                     (void **) &device));

  n_planes = gdk_memory_format_get_n_planes (texture->format);
  gdk_d3d12_resource_get_layout (self->resource,
                                 texture->format,
                                 &buffer_layout,
                                 footprints);

  hr_warn (ID3D12Device_CreateCommittedResource (device,
                                                 (&(D3D12_HEAP_PROPERTIES) {
                                                     .Type = D3D12_HEAP_TYPE_READBACK,
                                                     .CreationNodeMask = 1,
                                                     .VisibleNodeMask = 1,
                                                 }),
                                                 supports_nonzero (device) ? D3D12_HEAP_FLAG_CREATE_NOT_ZEROED
                                                                           : D3D12_HEAP_FLAG_NONE,
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
                                                 D3D12_RESOURCE_STATE_COPY_DEST,
                                                 NULL,
                                                 &IID_ID3D12Resource,
                                                 (void **) &buffer));

  hr_warn (ID3D12Device_CreateCommandQueue (device,
                                            (&(D3D12_COMMAND_QUEUE_DESC) {
                                                .Type = D3D12_COMMAND_LIST_TYPE_COPY,
                                                .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                                .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                            }),
                                            &IID_ID3D12CommandQueue,
                                            (void **) &queue));
  hr_warn (ID3D12Device_CreateCommandAllocator (device,
                                                D3D12_COMMAND_LIST_TYPE_COPY,
                                                &IID_ID3D12CommandAllocator,
                                                (void **) &allocator));
  hr_warn (ID3D12Device_CreateCommandList (device,
                                           0,
                                           D3D12_COMMAND_LIST_TYPE_COPY,
                                           allocator,
                                           NULL,
                                           &IID_ID3D12GraphicsCommandList,
                                           (void **) &commands));

  for (p = 0; p < n_planes; p++)
    {
      ID3D12GraphicsCommandList_CopyTextureRegion (commands,
                                                   (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                       .pResource = buffer,
                                                       .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                                                       .PlacedFootprint = footprints[p],
                                                   }),
                                                   0, 0, 0,
                                                   (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                       .pResource = self->resource,
                                                       .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                                       .SubresourceIndex = p,
                                                   }),
                                                   NULL);
    }
  hr_warn (ID3D12GraphicsCommandList_Close (commands));

  if (self->fence)
    ID3D12CommandQueue_Wait (queue, self->fence, self->fence_wait);

  ID3D12CommandQueue_ExecuteCommandLists (queue, 1, (ID3D12CommandList **) &commands);

  gdk_d3d12_command_queue_wait_sync (queue);

  hr_warn (ID3D12Resource_Map (buffer,
                               0,
                               (&(D3D12_RANGE) {
                                   .Begin = 0,
                                   .End = buffer_layout.size,
                               }),
                               &buffer_data));

  gdk_memory_convert (data,
                      layout,
                      color_state,
                      buffer_data,
                      &buffer_layout,
                      gdk_texture_get_color_state (texture));

  ID3D12Resource_Unmap (buffer, 0, (&(D3D12_RANGE) { 0, 0 }));

  gdk_win32_com_clear (&buffer);
  gdk_win32_com_clear (&commands);
  gdk_win32_com_clear (&allocator);
  gdk_win32_com_clear (&queue);
  gdk_win32_com_clear (&device);
}

static void
gdk_d3d12_texture_class_init (GdkD3D12TextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_d3d12_texture_download;

  gobject_class->dispose = gdk_d3d12_texture_dispose;
}

static void
gdk_d3d12_texture_init (GdkD3D12Texture *self)
{
}

GdkTexture *
gdk_d3d12_texture_new_from_builder (GdkD3D12TextureBuilder *builder,
                                    GDestroyNotify          destroy,
                                    gpointer                data,
                                    GError                **error)
{
  GdkD3D12Texture *self;
  GdkTexture *update_texture;
  GdkColorState *color_state;
  GdkMemoryFormat format;
  ID3D12Resource *resource;
  ID3D12Fence *fence;
  D3D12_RESOURCE_DESC desc;
  gboolean premultiplied;

  resource = gdk_d3d12_texture_builder_get_resource (builder);
  fence = gdk_d3d12_texture_builder_get_fence (builder);
  premultiplied = gdk_d3d12_texture_builder_get_premultiplied (builder);
  ID3D12Resource_GetDesc (resource, &desc);

  if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
      g_set_error (error,
                   GDK_D3D12_ERROR, GDK_D3D12_ERROR_UNSUPPORTED_FORMAT,
                   "Resource must be 2D texture");
      return NULL;
    }

  if (!gdk_memory_format_find_by_dxgi_format (desc.Format, premultiplied, &format))
    {
      g_set_error (error,
                   GDK_D3D12_ERROR, GDK_D3D12_ERROR_UNSUPPORTED_FORMAT,
                   "Unsupported DXGI format %u", desc.Format);
      return NULL;
    }

  /* FIXME: Do we need to validate the desc.SampleDesc? */

  color_state = gdk_d3d12_texture_builder_get_color_state (builder);
  if (color_state == NULL)
    color_state = gdk_color_state_get_srgb ();

  self = (GdkD3D12Texture *) g_object_new (GDK_TYPE_D3D12_TEXTURE,
                                           "width", (int) desc.Width,
                                           "height", (int) desc.Height,
                                           "color-state", color_state,
                                           NULL);

  GDK_TEXTURE (self)->format = format;
  ID3D12Resource_AddRef (resource);
  self->resource = resource;
  if (fence)
    {
      ID3D12Fence_AddRef (fence);
      self->fence = fence;
      self->fence_wait = gdk_d3d12_texture_builder_get_fence_wait (builder);
    }

  GDK_DEBUG (D3D12,
             "Creating %ux%u D3D12 texture, format %u",
             (UINT) desc.Width, desc.Height,
             desc.Format);

  /* Set this only once we know that the texture will be created.
   * Otherwise dispose() will run the callback */
  self->destroy = destroy;
  self->data = data;

  update_texture = gdk_d3d12_texture_builder_get_update_texture (builder);
  if (update_texture)
    {
      cairo_region_t *update_region = gdk_d3d12_texture_builder_get_update_region (builder);
      if (update_region)
        {
          cairo_rectangle_int_t tex_rect = { 0, 0,
                                             update_texture->width, update_texture->height };
          update_region = cairo_region_copy (update_region);
          cairo_region_intersect_rectangle (update_region, &tex_rect);
          gdk_texture_set_diff (GDK_TEXTURE (self), update_texture, update_region);
        }
    }

  return GDK_TEXTURE (self);

#if 0
  g_set_error_literal (error, GDK_D3D12_ERROR, GDK_D3D12_ERROR_NOT_AVAILABLE,
                       "d3d12 support disabled at compile-time.");
  return NULL;
#endif
}

ID3D12Resource *
gdk_d3d12_texture_get_resource (GdkD3D12Texture *self)
{
  return self->resource;
}

G_LOCK_DEFINE_STATIC(handle_creation);

HANDLE
gdk_d3d12_texture_get_resource_handle (GdkD3D12Texture *self)
{
  ID3D12Device *device = NULL;
  D3D12_HEAP_FLAGS heap_flags;
  HANDLE result;
  HRESULT hr;

  result = g_atomic_pointer_get (&self->resource_handle);
  if (result)
    return result;

  G_LOCK (handle_creation);

  result = g_atomic_pointer_get (&self->resource_handle);
  if (result)
    goto out;

  if (FAILED (ID3D12Resource_GetHeapProperties (self->resource, NULL, &heap_flags)) ||
      ((heap_flags & D3D12_HEAP_FLAG_SHARED) == 0))
    {
      GDK_DEBUG (D3D12, "Cannot export handle, heap is not shared");
      goto out;
    }

  if (FAILED (ID3D12Resource_GetDevice (self->resource,
                                        &IID_ID3D12Device,
                                        (void **) &device)))
    goto out;

  hr = ID3D12Device_CreateSharedHandle (device,
                                        (ID3D12DeviceChild *)self->resource,
                                        NULL,
                                        GENERIC_ALL,
                                        NULL,
                                        &result);
  if (FAILED (hr))
    {
      GDK_DEBUG (D3D12, "Failed to create shared handle for texture: %s",
                 g_win32_error_message (hr));
      goto out;
    }

  g_atomic_pointer_set (&self->resource_handle, result);

out:
  gdk_win32_com_clear (&device);

  G_UNLOCK (handle_creation);
  return result;
}

ID3D12Fence *
gdk_d3d12_texture_get_fence (GdkD3D12Texture *self)
{
  return self->fence;
}

HANDLE
gdk_d3d12_texture_get_fence_handle (GdkD3D12Texture *self)
{
  ID3D12Device *device = NULL;
  HANDLE result;
  HRESULT hr;

  if (self->fence == NULL)
    return NULL;

  result = g_atomic_pointer_get (&self->fence_handle);
  if (result)
    return result;

  G_LOCK (handle_creation);

  result = g_atomic_pointer_get (&self->fence_handle);
  if (result)
    goto out;

  if (FAILED (ID3D12Fence_GetDevice (self->fence,
                                     &IID_ID3D12Device,
                                     (void **) &device)))
    goto out;

  hr = ID3D12Device_CreateSharedHandle (device,
                                        (ID3D12DeviceChild *) self->fence,
                                        NULL,
                                        GENERIC_ALL,
                                        NULL,
                                        &result);
  if (FAILED (hr))
    {
      GDK_DEBUG (D3D12, "Failed to create shared handle for fence: %s",
                 g_win32_error_message (hr));
      goto out;
    }

  g_atomic_pointer_set (&self->fence_handle, result);

out:
  gdk_win32_com_clear (&device);

  G_UNLOCK (handle_creation);
  return result;
}

guint64
gdk_d3d12_texture_get_fence_wait (GdkD3D12Texture *self)
{
  return self->fence_wait;
}

/*
 * gdk_d3d12_texture_import_gl:
 * @self: texture to import into GL
 * @context: GL context to use for the import. The context
 *   must be the current context.
 * @out_mem_id: (out): out result for the memory object
 *   created during the import. See GL_EXT_memory_object_win32
 *   for details.
 * @out_semaphore_id: (out): out result for a semaphore
 *   created from the attached fence object. If no fence was
 *   provided, the value will be set to 0.
 *
 * Imports the given D3D12 texture into the given OpenGL
 * context.
 *
 * This function binds `GL_TEXTURE_2D` during the import.
 *
 * Returns: The newly created texture or 0 on failure.
 */
guint
gdk_d3d12_texture_import_gl (GdkD3D12Texture *self,
                             GdkGLContext    *context,
                             guint           *out_mem_id,
                             guint           *out_semaphore_id)
{
  GdkTexture *texture = GDK_TEXTURE (self);
  GLuint tex_id, mem_id, sema_id;
  D3D12_RESOURCE_DESC desc;
  HANDLE handle, fence_handle;
  GLint gl_internal_format, gl_internal_srgb_format;
  GLenum gl_format, gl_type;
  GLenum gl_error;
  GdkSwizzle gl_swizzle;

  /* We assume that the context is current, the caller needs to juggle that */
  g_assert (gdk_gl_context_get_current () == context);

  if (!gdk_gl_context_has_feature (context, GDK_GL_FEATURE_EXTERNAL_OBJECTS_WIN32))
    {
      GDK_DEBUG (D3D12, "Not importing %ux%u texture, EXT_external_objects_win32 is not supported",
                 texture->width, texture->height);
      return 0;
    }

  if (!gdk_memory_format_gl_format (texture->format,
                                    0,
                                    gdk_gl_context_get_use_es (context),
                                    &gl_internal_format,
                                    &gl_internal_srgb_format,
                                    &gl_format,
                                    &gl_type,
                                    &gl_swizzle))
    {
      GDK_DEBUG (D3D12, "Not importing %ux%u texture, format %s has no matching GL format",
                texture->width, texture->height,
                gdk_memory_format_get_name (texture->format));
      return 0;
    }

  handle = gdk_d3d12_texture_get_resource_handle (self);
  if (!handle)
    return 0;

  GDK_DEBUG (D3D12, "Attempting to import %ux%u texture",
             texture->width, texture->height);
  ID3D12Resource_GetDesc (self->resource, &desc);

  glCreateMemoryObjectsEXT (1, &mem_id);
  glImportMemoryWin32HandleEXT (mem_id,
                                0,
                                GL_HANDLE_TYPE_D3D12_RESOURCE_EXT,
                                handle);

  glGenTextures (1, &tex_id);
  glBindTexture (GL_TEXTURE_2D, tex_id);
  glTexStorageMem2DEXT (GL_TEXTURE_2D,
                        desc.MipLevels,
                        gl_internal_format,
                        desc.Width,
                        desc.Height,
                        mem_id,
                        0);

  fence_handle = gdk_d3d12_texture_get_resource_handle (self);
  if (fence_handle)
    {
      glGenSemaphoresEXT (1, &sema_id);
      glImportSemaphoreWin32HandleEXT (sema_id,
                                       GL_HANDLE_TYPE_D3D12_FENCE_EXT,
                                       fence_handle);
    }
  else
    sema_id = 0;

  gl_error = glGetError ();
  if (gl_error != GL_NO_ERROR)
    {
      GDK_DEBUG (D3D12, "Failed to import %ux%u texture, got GL error %u",
                 texture->width, texture->height,
                 gl_error);
      glDeleteMemoryObjectsEXT (1, &mem_id);
      glDeleteTextures (1, &tex_id);
      if (sema_id)
        glDeleteSemaphoresEXT (1, &sema_id);
      return 0;
    }

  *out_mem_id = mem_id;
  *out_semaphore_id = sema_id;

  return tex_id;
}
