/* GdkPixbuf library - GdkPixdata - functions for inlined pixbuf handling
 * Copyright (C) 1999, 2001 Tim Janik
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "gdk-pixdata.h"

#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-i18n.h"


/* --- functions --- */
static guint
pixdata_get_length (const GdkPixdata *pixdata)
{
  guint bpp, length;

  if ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB)
    bpp = 3;
  else if ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA)
    bpp = 4;
  else
    return 0;	/* invalid format */
  switch (pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK)
    {
      guint8 *rle_buffer;
      guint max_length;
    case GDK_PIXDATA_ENCODING_RAW:
      length = pixdata->rowstride * pixdata->height;
      break;
    case GDK_PIXDATA_ENCODING_RLE:
      /* need an RLE walk to determine size */
      max_length = pixdata->rowstride * pixdata->height;
      rle_buffer = pixdata->pixel_data;
      length = 0;
      while (length < max_length)
	{
	  guint chunk_length = *(rle_buffer++);

	  if (chunk_length & 128)
	    {
	      chunk_length = chunk_length - 128;
	      if (!chunk_length)	/* RLE data corrupted */
		return 0;
	      length += chunk_length * bpp;
	      rle_buffer += bpp;
	    }
	  else
	    {
	      if (!chunk_length)        /* RLE data corrupted */
		return 0;
	      chunk_length *= bpp;
	      length += chunk_length;
	      rle_buffer += chunk_length;
	    }
	}
      length = rle_buffer - pixdata->pixel_data;
      break;
    default:
      length = 0;
      break;
    }
  return length;
}

/**
 * gdk_pixdata_serialize:
 * @pixdata: A valid GdkPixdata structure to serialize
 * @stream_length_p: Location to store the resulting stream length in
 *
 * Serialize a #GdkPixdata structure into a byte stream.
 * The byte stream consists of a straight forward write out of the
 * #GdkPixdata fields in network byte order, plus the pixel_data
 * bytes the structure points to.
 *
 * Return value: A newly-allocated string containing the serialized
 * GdkPixdata structure
 **/
guint8* /* free result */
gdk_pixdata_serialize (const GdkPixdata *pixdata,
		       guint            *stream_length_p)
{
  guint8 *stream, *s;
  guint32 *istream;
  guint length;

  /* check args passing */
  g_return_val_if_fail (pixdata != NULL, NULL);
  g_return_val_if_fail (stream_length_p != NULL, NULL);
  /* check pixdata contents */
  g_return_val_if_fail (pixdata->magic == GDK_PIXBUF_MAGIC_NUMBER, NULL);
  g_return_val_if_fail (pixdata->width > 0, NULL);
  g_return_val_if_fail (pixdata->height > 0, NULL);
  g_return_val_if_fail (pixdata->rowstride >= pixdata->width, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ||
			(pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_SAMPLE_WIDTH_MASK) == GDK_PIXDATA_SAMPLE_WIDTH_8, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RAW ||
			(pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RLE, NULL);
  g_return_val_if_fail (pixdata->pixel_data != NULL, NULL);

  length = pixdata_get_length (pixdata);

  /* check length field */
  g_return_val_if_fail (length != 0, NULL);
  
  stream = g_malloc (GDK_PIXDATA_HEADER_LENGTH + length);
  istream = (guint32*) stream;

  /* store header */
  *istream++ = g_htonl (GDK_PIXBUF_MAGIC_NUMBER);
  *istream++ = g_htonl (GDK_PIXDATA_HEADER_LENGTH + length);
  *istream++ = g_htonl (pixdata->pixdata_type);
  *istream++ = g_htonl (pixdata->rowstride);
  *istream++ = g_htonl (pixdata->width);
  *istream++ = g_htonl (pixdata->height);

  /* copy pixel data */
  s = (guint8*) istream;
  memcpy (s, pixdata->pixel_data, length);
  s += length;

  *stream_length_p = GDK_PIXDATA_HEADER_LENGTH + length;
  g_assert (s - stream == *stream_length_p);	/* paranoid */

  return stream;
}

#define	return_header_corrupt(error)	{ \
  g_set_error (error, GDK_PIXBUF_ERROR, \
	       GDK_PIXBUF_ERROR_HEADER_CORRUPT, _("Image header corrupt")); \
  return FALSE; \
}
#define	return_invalid_format(error)	{ \
  g_set_error (error, GDK_PIXBUF_ERROR, \
	       GDK_PIXBUF_ERROR_UNKNOWN_FORMAT, _("Image format unknown")); \
  return FALSE; \
}
#define	return_pixel_corrupt(error)	{ \
  g_set_error (error, GDK_PIXBUF_ERROR, \
	       GDK_PIXBUF_ERROR_PIXEL_CORRUPT, _("Image pixel data corrupt")); \
  return FALSE; \
}

/**
 * gdk_pixdata_deserialize:
 * @pixdata: A GdkPixdata structure to be filled in
 * @stream_length: Length of the stream used for deserialization
 * @stream: Stream of bytes containing a serialized pixdata structure
 * @error: GError location to indicate failures (maybe NULL to ignore errors)
 *
 * Deserialize (reconstruct) a #GdkPixdata structure from a byte stream.
 * The byte stream consists of a straight forward write out of the
 * #GdkPixdata fields in network byte order, plus the pixel_data
 * bytes the structure points to.
 * The pixdata contents are reconstructed byte by byte and are checked
 * for validity, due to the checks, this function may prematurely abort
 * with errors, such as #GDK_PIXBUF_ERROR_HEADER_CORRUPT,
 * #GDK_PIXBUF_ERROR_PIXEL_CORRUPT or #GDK_PIXBUF_ERROR_UNKNOWN_FORMAT.
 *
 * Return value: Upon successfull deserialization #TRUE is returned,
 * #FALSE otherwise
 **/
gboolean
gdk_pixdata_deserialize (GdkPixdata   *pixdata,
			 guint         stream_length,
			 const guint8 *stream,
			 GError	     **error)
{
  guint32 *istream;
  guint color_type, sample_width, encoding;

  g_return_val_if_fail (pixdata != NULL, FALSE);
  if (stream_length < GDK_PIXDATA_HEADER_LENGTH)
    return_header_corrupt (error);
  g_return_val_if_fail (stream != NULL, FALSE);


  /* deserialize header */
  istream = (guint32*) stream;
  pixdata->magic = g_ntohl (*istream++);
  pixdata->length = g_ntohl (*istream++);
  if (pixdata->magic != GDK_PIXBUF_MAGIC_NUMBER || pixdata->length < GDK_PIXDATA_HEADER_LENGTH)
    return_header_corrupt (error);
  pixdata->pixdata_type = g_ntohl (*istream++);
  pixdata->rowstride = g_ntohl (*istream++);
  pixdata->width = g_ntohl (*istream++);
  pixdata->height = g_ntohl (*istream++);
  if (pixdata->width < 1 || pixdata->height < 1 ||
      pixdata->rowstride < pixdata->width)
    return_header_corrupt (error);
  color_type = pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK;
  sample_width = pixdata->pixdata_type & GDK_PIXDATA_SAMPLE_WIDTH_MASK;
  encoding = pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK;
  if ((color_type != GDK_PIXDATA_COLOR_TYPE_RGB &&
       color_type != GDK_PIXDATA_COLOR_TYPE_RGBA) ||
      sample_width != GDK_PIXDATA_SAMPLE_WIDTH_8 ||
      (encoding != GDK_PIXDATA_ENCODING_RAW &&
       encoding != GDK_PIXDATA_ENCODING_RLE))
    return_invalid_format (error);

  /* deserialize pixel data */
  if (stream_length < pixdata->length - GDK_PIXDATA_HEADER_LENGTH)
    return_pixel_corrupt (error);
  pixdata->pixel_data = (guint8*) istream;

  return TRUE;
}

static gboolean
diff2_rgb (guint8 *ip)
{
  return ip[0] != ip[3] || ip[1] != ip[4] || ip[2] != ip[5];
}

static gboolean
diff2_rgba (guint8 *ip)
{
  return ip[0] != ip[4] || ip[1] != ip[5] || ip[2] != ip[6] || ip[3] != ip[7];
}

static guint8*			/* dest buffer bound */
rl_encode_rgbx (guint8 *bp,	/* dest buffer */
		guint8 *ip,	/* image pointer */
		guint8 *limit,	/* image upper bound */
		guint   n_ch)
{
  gboolean (*diff2_pix) (guint8 *) = n_ch > 3 ? diff2_rgba : diff2_rgb;
  guint8 *ilimit = limit - n_ch;

  while (ip < limit)
    {
      g_assert (ip < ilimit); /* paranoid */

      if (diff2_pix (ip))
	{
	  guint8 *s_ip = ip;
	  guint l = 1;

	  ip += n_ch;
	  while (l < 127 && ip < ilimit && diff2_pix (ip))
	    { ip += n_ch; l += 1; }
	  if (ip == ilimit && l < 127)
	    { ip += n_ch; l += 1; }
	  *(bp++) = l;
	  memcpy (bp, s_ip, l * n_ch);
	  bp += l * n_ch;
	}
      else
	{
	  guint l = 2;

	  ip += n_ch;
	  while (l < 127 && ip < ilimit && !diff2_pix (ip))
	    { ip += n_ch; l += 1; }
	  *(bp++) = l | 128;
	  memcpy (bp, ip, n_ch);
	  ip += n_ch;
	  bp += n_ch;
	}
      if (ip == ilimit)
	{
	  *(bp++) = 1;
	  memcpy (bp, ip, n_ch);
	  ip += n_ch;
	  bp += n_ch;
	}
    }

  return bp;
}

gpointer
gdk_pixdata_from_pixbuf (GdkPixdata      *pixdata,
			 const GdkPixbuf *pixbuf,
			 gboolean         use_rle)
{
  gpointer free_me = NULL;
  guint height, rowstride, encoding, bpp, length;
  guint8 *img_buffer;

  g_return_val_if_fail (pixdata != NULL, NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (pixbuf->bits_per_sample == 8, NULL);
  g_return_val_if_fail ((pixbuf->n_channels == 3 && !pixbuf->has_alpha) ||
			(pixbuf->n_channels == 4 && pixbuf->has_alpha), NULL);
  g_return_val_if_fail (pixbuf->rowstride >= pixbuf->width, NULL);

  height = pixbuf->height;
  rowstride = pixbuf->rowstride;
  encoding = use_rle ? GDK_PIXDATA_ENCODING_RLE : GDK_PIXDATA_ENCODING_RAW;
  bpp = pixbuf->has_alpha ? 4 : 3;

  if (encoding == GDK_PIXDATA_ENCODING_RLE)
    {
      guint pad, n_bytes = rowstride * height;
      guint8 *img_buffer_end, *data;

      pad = rowstride;
      pad = MAX (pad, 130 + n_bytes / 127);
      data = g_new (guint8, pad + n_bytes);
      free_me = data;
      img_buffer = data;
      img_buffer_end = rl_encode_rgbx (img_buffer,
				       pixbuf->pixels, pixbuf->pixels + n_bytes,
				       bpp);
      length = img_buffer_end - img_buffer;
    }
  else
    {
      img_buffer = pixbuf->pixels;
      length = rowstride * height;
    }

  pixdata->magic = GDK_PIXBUF_MAGIC_NUMBER;
  pixdata->length = GDK_PIXDATA_HEADER_LENGTH + length;
  pixdata->pixdata_type = pixbuf->has_alpha ? GDK_PIXDATA_COLOR_TYPE_RGBA : GDK_PIXDATA_COLOR_TYPE_RGB;
  pixdata->pixdata_type |= GDK_PIXDATA_SAMPLE_WIDTH_8;
  pixdata->pixdata_type |= encoding;
  pixdata->rowstride = rowstride;
  pixdata->width = pixbuf->width;
  pixdata->height = height;
  pixdata->pixel_data = img_buffer;

  return free_me;
}

GdkPixbuf*
gdk_pixbuf_from_pixdata (const GdkPixdata *pixdata,
			 gboolean          copy_pixels,
			 GError          **error)
{
  guint encoding, bpp;
  guint8 *data = NULL;

  g_return_val_if_fail (pixdata != NULL, NULL);
  g_return_val_if_fail (pixdata->width > 0, NULL);
  g_return_val_if_fail (pixdata->height > 0, NULL);
  g_return_val_if_fail (pixdata->rowstride >= pixdata->width, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ||
			(pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_SAMPLE_WIDTH_MASK) == GDK_PIXDATA_SAMPLE_WIDTH_8, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RAW ||
			(pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RLE, NULL);
  g_return_val_if_fail (pixdata->pixel_data != NULL, NULL);

  bpp = (pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ? 3 : 4;
  encoding = pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK;
  if (encoding == GDK_PIXDATA_ENCODING_RLE)
    copy_pixels = TRUE;
  if (copy_pixels)
    {
      data = g_try_malloc (pixdata->rowstride * pixdata->height);
      if (!data)
	{
	  g_set_error (error, GDK_PIXBUF_ERROR,
		       GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
		       _("failed to allocate imagge buffer of %u bytes"),
		       pixdata->rowstride * pixdata->height);
	  return NULL;
	}
    }
  if (encoding == GDK_PIXDATA_ENCODING_RLE)
    {
      const guint8 *rle_buffer = pixdata->pixel_data;
      guint8 *image_buffer = data;
      guint8 *image_limit = data + pixdata->rowstride * pixdata->height;
      gboolean check_overrun = FALSE;

      while (image_buffer < image_limit)
	{
	  guint length = *(rle_buffer++);

	  if (length & 128)
	    {
	      length = length - 128;
	      check_overrun = image_buffer + length * bpp > image_limit;
	      if (check_overrun)
		length = (image_limit - image_buffer) / bpp;
	      if (bpp < 4)	/* RGB */
		do
		  {
		    memcpy (image_buffer, rle_buffer, 3);
		    image_buffer += 3;
		  }
		while (--length);
	      else		/* RGBA */
		do
		  {
		    memcpy (image_buffer, rle_buffer, 4);
		    image_buffer += 4;
		  }
		while (--length);
	      rle_buffer += bpp;
	    }
	  else
	    {
	      length *= bpp;
	      check_overrun = image_buffer + length > image_limit;
	      if (check_overrun)
		length = image_limit - image_buffer;
	      memcpy (image_buffer, rle_buffer, length);
	      image_buffer += length;
	      rle_buffer += length;
	    }
	}
      if (check_overrun)
	{
	  g_free (data);
	  g_set_error (error, GDK_PIXBUF_ERROR,
		       GDK_PIXBUF_ERROR_PIXEL_CORRUPT,
		       _("Image pixel data corrupt"));
	  return NULL;
	}
    }
  else if (copy_pixels)
    memcpy (data, pixdata->pixel_data, pixdata->rowstride * pixdata->height);
  else
    data = pixdata->pixel_data;

  return gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB,
				   (pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA,
				   8, pixdata->width, pixdata->height, pixdata->rowstride,
				   copy_pixels ? (GdkPixbufDestroyNotify) g_free : NULL, data);
}

typedef struct {
  /* config */
  gboolean     dump_stream;
  gboolean     dump_struct;
  gboolean     dump_macros;
  gboolean     dump_gtypes;
  gboolean     dump_rle_decoder;
  const gchar *static_prefix;
  const gchar *const_prefix;
  /* runtime */
  GString *gstring;
  guint    pos;
  gboolean pad;
} CSourceData;

static inline void
save_uchar (CSourceData *cdata,
	    guint8       d)
{
  GString *gstring = cdata->gstring;

  if (cdata->pos > 70)
    {
      if (cdata->dump_struct || cdata->dump_stream)
	{
	  g_string_append (gstring, "\"\n  \"");
	  cdata->pos = 3;
	  cdata->pad = FALSE;
	}
      if (cdata->dump_macros)
	{
	  g_string_append (gstring, "\" \\\n  \"");
	  cdata->pos = 3;
	  cdata->pad = FALSE;
	}
    }
  if (d < 33 || d > 126)
    {
      g_string_printfa (gstring, "\\%o", d);
      cdata->pos += 1 + 1 + (d > 7) + (d > 63);
      cdata->pad = d < 64;
      return;
    }
  if (d == '\\')
    {
      g_string_append (gstring, "\\\\");
      cdata->pos += 2;
    }
  else if (d == '"')
    {
      g_string_append (gstring, "\\\"");
      cdata->pos += 2;
    }
  else if (cdata->pad && d >= '0' && d <= '9')
    {
      g_string_append (gstring, "\"\"");
      g_string_append_c (gstring, d);
      cdata->pos += 3;
    }
  else
    {
      g_string_append_c (gstring, d);
      cdata->pos += 1;
    }
  cdata->pad = FALSE;
  return;
}

static inline void
save_rle_decoder (GString     *gstring,
		  const gchar *macro_name,
		  const gchar *s_uint,
		  const gchar *s_uint_8,
		  guint        n_ch)
{
  g_string_printfa (gstring, "#define %s_RUN_LENGTH_DECODE(image_buf, rle_data, size, bpp) do \\\n",
		    macro_name);
  g_string_printfa (gstring, "{ %s __bpp; %s *__ip; const %s *__il, *__rd; \\\n", s_uint, s_uint_8, s_uint_8);
  g_string_printfa (gstring, "  __bpp = (bpp); __ip = (image_buf); __il = __ip + (size) * __bpp; \\\n");
  
  g_string_printfa (gstring, "  __rd = (rle_data); if (__bpp > 3) { /* RGBA */ \\\n");
  
  g_string_printfa (gstring, "    while (__ip < __il) { %s __l = *(__rd++); \\\n", s_uint);
  g_string_printfa (gstring, "      if (__l & 128) { __l = __l - 128; \\\n");
  g_string_printfa (gstring, "        do { memcpy (__ip, __rd, 4); __ip += 4; } while (--__l); __rd += 4; \\\n");
  g_string_printfa (gstring, "      } else { __l *= 4; memcpy (__ip, __rd, __l); \\\n");
  g_string_printfa (gstring, "               __ip += __l; __rd += __l; } } \\\n");
  
  g_string_printfa (gstring, "  } else { /* RGB */ \\\n");
  
  g_string_printfa (gstring, "    while (__ip < __il) { %s __l = *(__rd++); \\\n", s_uint);
  g_string_printfa (gstring, "      if (__l & 128) { __l = __l - 128; \\\n");
  g_string_printfa (gstring, "        do { memcpy (__ip, __rd, 3); __ip += 3; } while (--__l); __rd += 3; \\\n");
  g_string_printfa (gstring, "      } else { __l *= 3; memcpy (__ip, __rd, __l); \\\n");
  g_string_printfa (gstring, "               __ip += __l; __rd += __l; } } \\\n");
  
  g_string_printfa (gstring, "  } } while (0)\n");
}

GString*
gdk_pixdata_to_csource (GdkPixdata        *pixdata,
			const gchar	  *name,
			GdkPixdataDumpType dump_type)
{
  CSourceData cdata = { 0, };
  gchar *s_uint_8, *s_uint_32, *s_uint, *s_char, *s_null;
  guint bpp, width, height, rowstride;
  gboolean rle_encoded;
  gchar *macro_name;
  guint8 *img_buffer, *img_buffer_end, *stream;
  guint stream_length;
  GString *gstring;
  
  /* check args passing */
  g_return_val_if_fail (pixdata != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  /* check pixdata contents */
  g_return_val_if_fail (pixdata->magic == GDK_PIXBUF_MAGIC_NUMBER, NULL);
  g_return_val_if_fail (pixdata->width > 0, NULL);
  g_return_val_if_fail (pixdata->height > 0, NULL);
  g_return_val_if_fail (pixdata->rowstride >= pixdata->width, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ||
			(pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGBA, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_SAMPLE_WIDTH_MASK) == GDK_PIXDATA_SAMPLE_WIDTH_8, NULL);
  g_return_val_if_fail ((pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RAW ||
			(pixdata->pixdata_type & GDK_PIXDATA_ENCODING_MASK) == GDK_PIXDATA_ENCODING_RLE, NULL);
  g_return_val_if_fail (pixdata->pixel_data != NULL, NULL);

  img_buffer = pixdata->pixel_data;
  if (pixdata->length < 1)
    img_buffer_end = img_buffer + pixdata_get_length (pixdata);
  else
    img_buffer_end = img_buffer + pixdata->length - GDK_PIXDATA_HEADER_LENGTH;
  g_return_val_if_fail (img_buffer < img_buffer_end, NULL);

  bpp = (pixdata->pixdata_type & GDK_PIXDATA_COLOR_TYPE_MASK) == GDK_PIXDATA_COLOR_TYPE_RGB ? 3 : 4;
  width = pixdata->width;
  height = pixdata->height;
  rowstride = pixdata->rowstride;
  rle_encoded = (pixdata->pixdata_type & GDK_PIXDATA_ENCODING_RLE) > 0;
  macro_name = g_strdup (name);
  g_strup (macro_name);

  cdata.dump_macros = (dump_type & GDK_PIXDATA_DUMP_MACROS) > 0;
  cdata.dump_struct = (dump_type & GDK_PIXDATA_DUMP_PIXDATA_STRUCT) > 0;
  cdata.dump_stream = !cdata.dump_macros && !cdata.dump_struct;
  g_return_val_if_fail (cdata.dump_macros + cdata.dump_struct + cdata.dump_stream == 1, NULL);

  cdata.dump_gtypes = (dump_type & GDK_PIXDATA_DUMP_CTYPES) == 0;
  cdata.dump_rle_decoder = (dump_type & GDK_PIXDATA_DUMP_RLE_DECODER) > 0;
  cdata.static_prefix = (dump_type & GDK_PIXDATA_DUMP_STATIC) ? "static " : "";
  cdata.const_prefix = (dump_type & GDK_PIXDATA_DUMP_CONST) ? "const " : "";
  gstring = g_string_new ("");
  cdata.gstring = gstring;

  if (!cdata.dump_macros && cdata.dump_gtypes)
    {
      s_uint_8 =  "guint8 ";
      s_uint_32 = "guint32";
      s_uint =    "guint  ";
      s_char =    "gchar  ";
      s_null =    "NULL";
    }
  else if (!cdata.dump_macros)
    {
      s_uint_8 =  "unsigned char";
      s_uint_32 = "unsigned int ";
      s_uint =    "unsigned int ";
      s_char =    "char         ";
      s_null =    "(char*) 0";
    }
  else if (cdata.dump_macros && cdata.dump_gtypes)
    {
      s_uint_8 =  "guint8";
      s_uint_32 = "guint32";
      s_uint  =   "guint";
      s_char =    "gchar";
      s_null =    "NULL";
    }
  else /* cdata.dump_macros && !cdata.dump_gtypes */
    {
      s_uint_8 =  "unsigned char";
      s_uint_32 = "unsigned int";
      s_uint =    "unsigned int";
      s_char =    "char";
      s_null =    "(char*) 0";
    }

  /* initial comment
   */
  g_string_printfa (gstring,
		    "/* GdkPixbuf %s C-Source image dump %s*/\n\n",
		    bpp > 3 ? "RGBA" : "RGB",
		    rle_encoded ? "1-byte-run-length-encoded " : "");

  /* dump RLE decoder for structures
   */
  if (cdata.dump_rle_decoder && cdata.dump_struct)
    save_rle_decoder (gstring,
		      macro_name,
		      cdata.dump_gtypes ? "guint" : "unsigned int",
		      cdata.dump_gtypes ? "guint8" : "unsigned char",
		      bpp);

  /* format & size blurbs
   */
  if (cdata.dump_macros)
    {
      g_string_printfa (gstring, "#define %s_ROWSTRIDE (%u)\n",
			macro_name, rowstride);
      g_string_printfa (gstring, "#define %s_WIDTH (%u)\n",
			macro_name, width);
      g_string_printfa (gstring, "#define %s_HEIGHT (%u)\n",
			macro_name, height);
      g_string_printfa (gstring, "#define %s_BYTES_PER_PIXEL (%u) /* 3:RGB, 4:RGBA */\n",
			macro_name, bpp);
    }
  if (cdata.dump_struct)
    {
      g_string_printfa (gstring, "%s%sGdkPixdata %s = {\n",
			cdata.static_prefix, cdata.const_prefix, name);
      g_string_printfa (gstring, "  0x%x, /* Pixbuf magic: 'GdkP' */\n",
			GDK_PIXBUF_MAGIC_NUMBER);
      g_string_printfa (gstring, "  %u + %u, /* header length + pixel_data length */\n",
			GDK_PIXDATA_HEADER_LENGTH,
			rle_encoded ? img_buffer_end - img_buffer : rowstride * height);
      g_string_printfa (gstring, "  0x%x, /* pixdata_type */\n",
			pixdata->pixdata_type);
      g_string_printfa (gstring, "  %u, /* rowstride */\n",
			rowstride);
      g_string_printfa (gstring, "  %u, /* width */\n",
			width);
      g_string_printfa (gstring, "  %u, /* height */\n",
			height);
      g_string_printfa (gstring, "  /* pixel_data: */\n");
    }
  if (cdata.dump_stream)
    {
      guint pix_length = img_buffer_end - img_buffer;
      
      stream = gdk_pixdata_serialize (pixdata, &stream_length);
      img_buffer_end = stream + stream_length;

      g_string_printfa (gstring, "%s%s%s %s[] = \n",
			cdata.static_prefix, cdata.const_prefix,
			cdata.dump_gtypes ? "guint8" : "unsigned char",
			name);
      g_string_printfa (gstring, "( \"\"\n  /* Pixbuf magic (0x%x) */\n  \"",
			GDK_PIXBUF_MAGIC_NUMBER);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* length: header (%u) + pixel_data (%u) */\n  \"",
			GDK_PIXDATA_HEADER_LENGTH,
			rle_encoded ? pix_length : rowstride * height);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* pixdata_type (0x%x) */\n  \"",
			pixdata->pixdata_type);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* rowstride (%u) */\n  \"",
			rowstride);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* width (%u) */\n  \"", width);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* height (%u) */\n  \"", height);
      cdata.pos = 3;
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      save_uchar (&cdata, *stream++); save_uchar (&cdata, *stream++);
      g_string_printfa (gstring, "\"\n  /* pixel_data: */\n");
      img_buffer = stream;
    }

  /* pixel_data intro
   */
  if (cdata.dump_macros)
    {
      g_string_printfa (gstring, "#define %s_%sPIXEL_DATA ((%s*) \\\n",
			macro_name,
			rle_encoded ? "RLE_" : "",
			s_uint_8);
      g_string_printfa (gstring, "  \"");
      cdata.pos = 2;
    }
  if (cdata.dump_struct)
    {
      g_string_printfa (gstring, "  \"");
      cdata.pos = 3;
    }
  if (cdata.dump_stream)
    {
      g_string_printfa (gstring, "  \"");
      cdata.pos = 3;
    }
    
  /* pixel_data
   */
  do
    save_uchar (&cdata, *img_buffer++);
  while (img_buffer < img_buffer_end);

  /* pixel_data trailer
   */
  if (cdata.dump_macros)
    g_string_printfa (gstring, "\")\n\n");
  if (cdata.dump_struct)
    g_string_printfa (gstring, "\",\n};\n\n");
  if (cdata.dump_stream)
    g_string_printfa (gstring, "\");\n\n");

  /* dump RLE decoder for macros
   */
  if (cdata.dump_rle_decoder && cdata.dump_macros)
    save_rle_decoder (gstring,
		      macro_name,
		      cdata.dump_gtypes ? "guint" : "unsigned int",
		      cdata.dump_gtypes ? "guint8" : "unsigned char",
		      bpp);

  /* cleanup
   */
  g_free (macro_name);
  
  return gstring;
}

/**
 * gdk_pixbuf_new_from_stream:
 * @stream_length: Length in bytes of the @stream argument
 * @stream: Byte stream containing a serialized GdkPixdata structure
 * @copy_pixels: Whether to copy the pixels data, or keep direct pointers into the
 *               stream data for the resulting pixbuf
 * @error: GError return location, may be NULL to ignore errors
 *
 * Create a #GdkPixbuf from a serialized GdkPixdata structure.
 * Gtk+ ships with a program called gdk-pixbuf-csource which allowes
 * for conversion of #GdkPixbufs into various kinds of C sources.
 * This is useful if you want to ship a program with images, but
 * don't want to depend on any external files.
 *
 * Since the inline pixbuf is read-only static data, you
 * don't need to copy it unless you intend to write to it.
 * For deserialization of streams that are run length encoded, the
 * decoded data is always a copy of the input stream.
 *
 * If you create a pixbuf from const inline data compiled into your
 * program, it's probably safe to ignore errors, since things will
 * always succeed.  For non-const inline data, you could get out of
 * memory. For untrusted inline data located at runtime, you could
 * have corrupt inline data in addition.
 *
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1, or NULL If error is set.
 **/
GdkPixbuf*
gdk_pixbuf_new_from_stream (gint          stream_length,
			    const guint8 *stream,
			    gboolean      copy_pixels,
			    GError      **error)
{
  GdkPixdata pixdata;

  if (stream_length != -1)
    g_return_val_if_fail (stream_length > GDK_PIXDATA_HEADER_LENGTH, NULL);
  g_return_val_if_fail (stream != NULL, NULL);

  if (!gdk_pixdata_deserialize (&pixdata, stream_length, stream, error))
    return NULL;

  return gdk_pixbuf_from_pixdata (&pixdata, copy_pixels, error);
}
