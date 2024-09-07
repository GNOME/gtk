#include <gtk.h>
#include <avif/avif.h>

gboolean
gdk_texture_save_to_avif (GtkTexture *texture,
                          const char *filename)
{
  avifEncoder *encoder;
  avifImage *image;
  avifRWData output = { NULL, 0 };
  gboolean res;
  uint32_t depth;
  avifPixelFormat pixelFormat;

  switch (gdk_texture_get_format (texture))
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
      depth = 8;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
      depth = 8;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
      depth = 8;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_B8G8R8A8:
      depth = 8;
      break;
    case GDK_MEMORY_A8R8G8B8:
      depth = 8;
      break;
    case GDK_MEMORY_R8G8B8A8:
      depth = 8;
      break;
    case GDK_MEMORY_A8B8G8R8:
      depth = 8;
      break;
    case GDK_MEMORY_R8G8B8:
      depth = 8;
      break;
    case GDK_MEMORY_B8G8R8:
      depth = 8;
      break;
    case GDK_MEMORY_R16G16B16:
      depth = 16;
      break;
    case GDK_MEMORY_R16G16B16A16_PREMULTIPLIED:
      depth = 16;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_R16G16B16A16:
      depth = 16;
      break;
    case GDK_MEMORY_R16G16B16_FLOAT:
      depth = 16;
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT_PREMULTIPLIED:
      depth = 16;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_R16G16B16A16_FLOAT:
      depth = 16;
      break;
    case GDK_MEMORY_R32G32B32_FLOAT:
      depth = 32;
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED:
      depth = 32;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_R32G32B32A32_FLOAT:
      depth = 32;
      break;
    case GDK_MEMORY_G8A8_PREMULTIPLIED:
      depth = 32;
      break;
    case GDK_MEMORY_G8A8:
      depth = 8;
      break;
    case GDK_MEMORY_G8:
      depth = 8;
      break;
    case GDK_MEMORY_G16A16_PREMULTIPLIED:
      depth = 16;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_G16A16:
      depth = 16;
      break;
    case GDK_MEMORY_G16:
      depth = 16;
      break;
    case GDK_MEMORY_A8:
      depth = 8;
      break;
    case GDK_MEMORY_A16:
      depth = 16;
      break;
    case GDK_MEMORY_A16_FLOAT:
      depth = 16;
      break;
    case GDK_MEMORY_A32_FLOAT:
      depth = 32;
      break;
    case GDK_MEMORY_A8B8G8R8_PREMULTIPLIED:
      depth = 8;
      premultiplied = TRUE;
      break;
    case GDK_MEMORY_B8G8R8X8:
      depth = 8;
      break;
    case GDK_MEMORY_X8R8G8B8:
      depth = 8;
      break;
    case GDK_MEMORY_R8G8B8X8:
      depth = 8;
      break;
    case GDK_MEMORY_X8B8G8R8:
      depth = 8;
      break;
    }

  image = avifImageCreate (gdk_texture_get_width (texture),
                           gdk_texture_get_height (texture),
                           depth,
                           pixelFormat);

  encoder = avifEncoderCreate ();
  avifEncoderWrite (encoder, image, &output);
  avifEncoderDestroy (encoder);

  res = g_file_set_contents (filename, output.data, output.size, NULL);

  g_free (output.data);

  return res;
}

GdkTexture *
gdk_load_avif (GBytes *bytes)
{
  avifDecoder *decoder;
  GdkTexture *texture;

  decoder = avifDecoderCreate ();

  avifDecoderSetSource (decoder, AVIF_DECODER_SOURCE_PRIMARY_ITEM);

  decoder->ignoreExif = 1;
  decoder->ignoreXMP = 1;

  if (avifDecoderSetIOMemory (decoder, g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes)) != AVIF_RESULT_OK)
    {
      avifDecoderDestroy (decoder);
      return NULL;
    }

  if (avifDecoderParse (decoder) != AVIF_RESULT_OK)
    {
      avifDecoderDestroy (decoder);
      return NULL;
    }

  builder = gdk_memory_texture_builder_new ();

  gdk_memory_texture_builder_set_width (builder, decoder->image->width);
  gdk_memory_texture_builder_set_height (builder, decoder->image->height);

  GdkMemoryFormat format;

  if (decoder->image->alphaPlane)
    {
      if (decoder->image->alpha->alphaPremultiplied)
        {
          if (decoder->image->depth > 8)
            format = GDK_MEMORY_R16G16B16A16_PREMULTIPLIED;
          else
            format = GDK_MEMORY_R8G8B8A8_PREMULTIPLIED;
        }
      else
        {
          if (decoder->image->depth > 8)
            format = GDK_MEMORY_R16G16B16A16;
          else
            format = GDK_MEMORY_R8G8B8A8;
        }
    }
  else
    {
      if (decoder->image->depth > 8)
        format = GDK_MEMORY_R16G16B16;
      else
        format = GDK_MEMORY_R8G8B8;
    }

  gdk_memory_texture_builder_set_format (builder, format);

// Now available:
    // * All decoder->image information other than pixel data:
    //   * width, height, depth
    //   * transformations (pasp, clap, irot, imir)
    //   * color profile (icc, CICP)
    //   * metadata (Exif, XMP)
    // * decoder->alphaPresent
    // * number of total images in the AVIF (decoder->imageCount)
    // * overall image sequence timing (including per-frame timing with avifDecoderNthImageTiming())

  avifDecorderDestroy (decoder);

  return texture;
}
