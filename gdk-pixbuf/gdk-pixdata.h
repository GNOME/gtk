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
#ifndef __GDK_PIXDATA_H__
#define __GDK_PIXDATA_H__

#include        <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GDK_PIXBUF_MAGIC_NUMBER (0x47646b50)    /* 'GdkP' */

typedef enum
{
  /* colorspace + alpha */
  GDK_PIXDATA_COLOR_TYPE_RGB    = 0x01,
  GDK_PIXDATA_COLOR_TYPE_RGBA   = 0x02,
  GDK_PIXDATA_COLOR_TYPE_MASK   = 0xff,
  /* width, support 8bits only currently */
  GDK_PIXDATA_SAMPLE_WIDTH_8    = 0x01 << 16,
  GDK_PIXDATA_SAMPLE_WIDTH_MASK = 0x0f << 16,
  /* encoding */
  GDK_PIXDATA_ENCODING_RAW      = 0x01 << 24,
  GDK_PIXDATA_ENCODING_RLE      = 0x02 << 24,
  GDK_PIXDATA_ENCODING_MASK     = 0x0f << 24
} GdkPixdataType;

typedef struct _GdkPixdata GdkPixdata;
struct _GdkPixdata
{
  guint32 magic;        /* GDK_PIXBUF_MAGIC_NUMBER */
  gint32  length;       /* <1 to disable length checks, otherwise:
			 * GDK_PIXDATA_HEADER_LENGTH + pixel_data length
			 */
  guint32 pixdata_type; /* GdkPixdataType */
  guint32 rowstride;    /* maybe 0 to indicate non-padded data */
  guint32 width;
  guint32 height;
  guint8 *pixel_data;
};
#define	GDK_PIXDATA_HEADER_LENGTH	(4 + 4 + 4 + 4 + 4 + 4)

/* the returned stream is plain htonl of GdkPixdata members + pixel_data */
guint8*		gdk_pixdata_serialize	(const GdkPixdata	*pixdata,
					 guint			*stream_length_p);
gboolean	gdk_pixdata_deserialize	(GdkPixdata		*pixdata,
					 guint			 stream_length,
					 const guint8		*stream,
					 GError		       **error);
gpointer	gdk_pixdata_from_pixbuf	(GdkPixdata		*pixdata,
					 const GdkPixbuf	*pixbuf,
					 gboolean		 use_rle);
GdkPixbuf*	gdk_pixbuf_from_pixdata	(const GdkPixdata	*pixdata,
					 gboolean		 copy_pixels,
					 GError		       **error);
typedef enum
{
  /* type of source to save */
  GDK_PIXDATA_DUMP_PIXDATA_STREAM	= 0,
  GDK_PIXDATA_DUMP_PIXDATA_STRUCT	= 1,
  GDK_PIXDATA_DUMP_MACROS		= 2,
  /* type of variables to use */
  GDK_PIXDATA_DUMP_GTYPES		= 0,
  GDK_PIXDATA_DUMP_CTYPES		= 1 << 8,
  GDK_PIXDATA_DUMP_STATIC		= 1 << 9,
  GDK_PIXDATA_DUMP_CONST		= 1 << 10,
  /* save RLE decoder macro? */
  GDK_PIXDATA_DUMP_RLE_DECODER		= 1 << 16,
} GdkPixdataDumpType;
  

GString*	gdk_pixdata_to_csource	(GdkPixdata		*pixdata,
					 const gchar		*name,
					 GdkPixdataDumpType	 dump_type);



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GDK_PIXDATA_H__ */
