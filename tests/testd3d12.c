#include <gtk/gtk.h>

#include <gdk/win32/gdkwin32.h>

#include "gtkclipperprivate.h"

static char *
supported_formats_to_string (void)
{
  return g_strdup ("whatever");
}

static GdkTexture *
make_d3d12_texture (ID3D12Device *device,
                    GdkTexture   *texture,
                    guint32       format,
                    gboolean      disjoint,
                    gboolean      premultiplied,
                    gboolean      flip)
{
  GdkTextureDownloader *downloader;
  UINT64 buffer_size;
  GdkD3D12TextureBuilder *builder;
  GError *error = NULL;
  HRESULT hr;
  ID3D12CommandAllocator *allocator;
  ID3D12GraphicsCommandList *commands;
  ID3D12CommandQueue *queue;
  ID3D12Resource *buffer, *resource;
  D3D12_RESOURCE_DESC resource_desc;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
  void *buffer_data;

  hr = ID3D12Device_CreateCommittedResource (device,
                                             (&(D3D12_HEAP_PROPERTIES) {
                                                 .Type = D3D12_HEAP_TYPE_DEFAULT,
                                                 .CreationNodeMask = 1,
                                                 .VisibleNodeMask = 1,
                                             }),
                                             D3D12_HEAP_FLAG_SHARED, 
                                             (&(D3D12_RESOURCE_DESC) {
                                                 .Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D,
                                                 .Width = gdk_texture_get_width (texture),
                                                 .Height = gdk_texture_get_height (texture),
                                                 .DepthOrArraySize = 1,
                                                 .MipLevels = 0,
                                                 .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                                                 .SampleDesc = {
                                                     .Count = 1,
                                                     .Quality = 0,
                                                 },
                                                 .Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
                                                 .Flags = D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS,
                                             }),
                                             D3D12_RESOURCE_STATE_COPY_DEST, 
                                             NULL,
                                             &IID_ID3D12Resource,
                                             (void **) &resource);
  g_assert (SUCCEEDED (hr));

  ID3D12Resource_GetDesc (resource, &resource_desc);
  ID3D12Device_GetCopyableFootprints (device,
                                      &resource_desc,
                                      0, 1, 0,
                                      &footprint,
                                      NULL,
                                      NULL,
                                      NULL);
  buffer_size = footprint.Footprint.RowPitch * footprint.Footprint.Height;

  hr = ID3D12Device_CreateCommittedResource (device,
                                             (&(D3D12_HEAP_PROPERTIES) {
                                                 .Type = D3D12_HEAP_TYPE_UPLOAD,
                                                 .CreationNodeMask = 1,
                                                 .VisibleNodeMask = 1,
                                             }),
                                             D3D12_HEAP_FLAG_NONE,
                                             (&(D3D12_RESOURCE_DESC) {
                                                 .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
                                                 .Width = buffer_size,
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
  g_assert (SUCCEEDED (hr));

  ID3D12Resource_Map (buffer, 0, (&(D3D12_RANGE) { 0, buffer_size }), &buffer_data );

  downloader = gdk_texture_downloader_new (texture);
  if (premultiplied)
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8_PREMULTIPLIED);
  else
    gdk_texture_downloader_set_format (downloader, GDK_MEMORY_R8G8B8A8);
  gdk_texture_downloader_download_into (downloader, buffer_data, footprint.Footprint.RowPitch);
  gdk_texture_downloader_free (downloader);

  if (flip)
    {
      int x, y, width, height, stride;
      width = gdk_texture_get_width (texture);
      height = gdk_texture_get_height (texture);
      stride = footprint.Footprint.RowPitch;
      for (y = 0; y < height; y++)
        {
          guint32 *row = (guint32 *) ((guint8 *) buffer_data + y * stride);
          for (x = 0; x < width / 2; x++)
            {
              guint32 p = row[x];
              row[x] = row[width - 1 - x];
              row[width - 1 - x] = p;
            }
        }
    }

  ID3D12Resource_Unmap (buffer, 0, (&(D3D12_RANGE) { 0, buffer_size }));
  
  hr = ID3D12Device_CreateCommandAllocator (device,
                                            D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            &IID_ID3D12CommandAllocator,
                                            (void **) &allocator);
  g_assert (SUCCEEDED (hr));

  hr = ID3D12Device_CreateCommandList (device,
                                       0,
                                       D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       allocator,
                                       NULL,
                                       &IID_ID3D12GraphicsCommandList,
                                       (void **) &commands);
  g_assert (SUCCEEDED (hr));

  ID3D12GraphicsCommandList_CopyTextureRegion (commands,
                                               (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                   .pResource = resource,
                                                   .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
                                                   .SubresourceIndex = 0,
                                               }),
                                               0, 0, 0, 
                                               (&(D3D12_TEXTURE_COPY_LOCATION) {
                                                   .pResource = buffer,
                                                   .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
                                                   .PlacedFootprint = footprint,
                                               }),
                                               NULL);
  hr = ID3D12GraphicsCommandList_Close (commands);
  g_assert (SUCCEEDED (hr));

  hr = ID3D12Device_CreateCommandQueue (device,
                                        (&(D3D12_COMMAND_QUEUE_DESC) {
                                            .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            .Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
                                            .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
                                        }),
                                        &IID_ID3D12CommandQueue,
                                        (void **) &queue);
  g_assert (SUCCEEDED (hr));

  ID3D12CommandQueue_ExecuteCommandLists (queue, 1, (ID3D12CommandList **) &commands);

  builder = gdk_d3d12_texture_builder_new ();

  gdk_d3d12_texture_builder_set_resource (builder, resource);
  gdk_d3d12_texture_builder_set_premultiplied (builder, premultiplied);

  texture = gdk_d3d12_texture_builder_build (builder, NULL, NULL, &error);
  if (!texture)
    g_error ("Failed to create d3d12 texture: %s", error->message);

  g_object_unref (builder);
  ID3D12Resource_Release (buffer);
  ID3D12Resource_Release (resource);
  ID3D12GraphicsCommandList_Release (commands);
  ID3D12CommandAllocator_Release (allocator);
  ID3D12CommandQueue_Release (queue);

  return texture;
}

G_GNUC_NORETURN
static void
usage (void)
{
  char *formats = supported_formats_to_string ();
  g_print ("Usage: testdmabuf [--undecorated][--disjoint][--download-to FILE][--padding PADDING] FORMAT FILE\n"
           "Supported formats: %s\n", formats);
  g_free (formats);
  exit (1);
}

static gboolean
toggle_fullscreen (GtkWidget *widget,
                   GVariant  *args,
                   gpointer   data)
{
  GtkWindow *window = GTK_WINDOW (widget);

  if (gtk_window_is_fullscreen (window))
    gtk_window_unfullscreen (window);
  else
    gtk_window_fullscreen (window);

  return TRUE;
}

static gboolean
toggle_overlay (GtkWidget *widget,
                GVariant  *args,
                gpointer   data)
{
  static GtkWidget *child = NULL;
  GtkOverlay *overlay = (GtkOverlay *) data;

  if (child)
    {
      gtk_overlay_remove_overlay (overlay, child);
      child = NULL;
    }
  else
    {
      GtkWidget *spinner;
      spinner = gtk_spinner_new ();
      gtk_spinner_start (GTK_SPINNER (spinner));
      child = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE);
      gtk_box_append (GTK_BOX (child), spinner);
      gtk_box_append (GTK_BOX (child), gtk_image_new_from_icon_name ("media-playback-start-symbolic"));
      gtk_widget_set_halign (child, GTK_ALIGN_CENTER);
      gtk_widget_set_valign (child, GTK_ALIGN_CENTER);
      gtk_overlay_add_overlay (overlay, child);
    }

  return TRUE;
}

static GdkTexture *texture;
static GdkTexture *texture_flipped;

static gboolean
toggle_flip (GtkWidget *widget,
             GVariant  *args,
             gpointer   data)
{
  GtkPicture *picture = (GtkPicture *) data;

  if (!texture_flipped)
    return FALSE;

  if (gtk_picture_get_paintable (picture) == GDK_PAINTABLE (texture))
    gtk_picture_set_paintable (picture, GDK_PAINTABLE (texture_flipped));
  else
    gtk_picture_set_paintable (picture, GDK_PAINTABLE (texture));

  return TRUE;
}

static gboolean
toggle_start (GtkWidget *widget,
              GVariant  *args,
              gpointer   data)
{
  GtkWidget *offload = (GtkWidget *) data;

  if (gtk_widget_get_halign (offload) == GTK_ALIGN_CENTER)
    gtk_widget_set_halign (offload, GTK_ALIGN_START);
  else
    gtk_widget_set_halign (offload, GTK_ALIGN_CENTER);

  return TRUE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *offload, *picture, *overlay;
  char *filename;
  guint32 format;
  gboolean disjoint = FALSE;
  gboolean premultiplied = TRUE;
  gboolean decorated = TRUE;
  gboolean fullscreen = FALSE;
  unsigned int i;
  const char *save_filename = NULL;
  GtkEventController *controller;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GtkShortcut *shortcut;
  GdkPaintable *paintable;
  GdkTexture *orig;
  ID3D12Device *device;
  HRESULT hr;
  int padding[4] = { 0, }; /* left, right, top, bottom */
  int padding_set = 0;

  for (i = 1; i < argc; i++)
    {
      if (g_str_equal (argv[i], "--disjoint"))
        disjoint = TRUE;
      else if (g_str_equal (argv[i], "--undecorated"))
        decorated = FALSE;
      else if (g_str_equal (argv[i], "--fullscreen"))
        fullscreen = TRUE;
      else if (g_str_equal (argv[i], "--unpremultiplied"))
        premultiplied = FALSE;
      else if (g_str_equal (argv[i], "--download-to"))
        {
          i++;
          if (i == argc)
            usage ();

          save_filename = argv[i];
        }
      else if (g_str_equal (argv[i], "--padding"))
        {
          if (padding_set < 4)
            {
              char **strv;

              i++;
              if (i == argc)
                usage ();

              strv = g_strsplit (argv[i], ",", 0);
              if (g_strv_length (strv) > 4)
                g_error ("Too much padding");

              for (padding_set = 0; padding_set < 4; padding_set++)
                {
                  guint64 num;
                  GError *error = NULL;

                  if (!strv[padding_set])
                    break;

                  if (!g_ascii_string_to_unsigned (strv[padding_set], 10, 0, 100, &num, &error))
                    g_error ("%s", error->message);

                  padding[padding_set] = (int) num;
               }
            }
          else
            g_error ("Too much padding");
        }
      else
        break;
    }

  if (argc - i != 2)
    {
      usage ();
      return 1;
    }

  filename = argv[argc - 1];

  gtk_init ();

  hr = D3D12CreateDevice (NULL, D3D_FEATURE_LEVEL_12_0, &IID_ID3D12Device, (void **) &device);
  g_assert (SUCCEEDED (hr));

  orig = gdk_texture_new_from_filename (filename, NULL);
  format = strtoul (argv[argc-2], NULL, 10);
  texture = make_d3d12_texture (device, orig, format, disjoint, premultiplied, FALSE);
  texture_flipped = make_d3d12_texture (device, orig, format, disjoint, premultiplied, TRUE);
  g_object_unref (orig);

  if (padding_set > 0)
    {
      paintable = gtk_clipper_new (GDK_PAINTABLE (texture),
                                   &GRAPHENE_RECT_INIT (padding[0],
                                                        padding[2],
                                                        gdk_texture_get_width (texture) - padding[0] - padding[1],
                                                        gdk_texture_get_height (texture) - padding[2] - padding[3]));
    }
  else
    paintable = GDK_PAINTABLE (texture);

  if (save_filename)
    gdk_texture_save_to_png (texture, save_filename);

  window = gtk_window_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), decorated);
  if (fullscreen)
    gtk_window_fullscreen (GTK_WINDOW (window));

  picture = gtk_picture_new_for_paintable (paintable);
  offload = gtk_graphics_offload_new (picture);
  gtk_widget_set_halign (offload, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (offload, GTK_ALIGN_CENTER);
  overlay = gtk_overlay_new ();

  gtk_overlay_set_child (GTK_OVERLAY (overlay), offload);
  gtk_window_set_child (GTK_WINDOW (window), overlay);

  controller = gtk_shortcut_controller_new ();

  trigger = gtk_keyval_trigger_new (GDK_KEY_F11, GDK_NO_MODIFIER_MASK);
  action = gtk_callback_action_new (toggle_fullscreen, NULL, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_O, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_overlay, overlay, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_F, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_flip, picture, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  trigger = gtk_keyval_trigger_new (GDK_KEY_S, GDK_CONTROL_MASK);
  action = gtk_callback_action_new (toggle_start, offload, NULL);
  shortcut = gtk_shortcut_new (trigger, action);
  gtk_shortcut_controller_add_shortcut (GTK_SHORTCUT_CONTROLLER (controller), shortcut);

  gtk_widget_add_controller (window, controller);

  gtk_window_present (GTK_WINDOW (window));

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
