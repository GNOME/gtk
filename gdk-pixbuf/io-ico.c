/* GdkPixbuf library - Windows Icon/Cursor image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * Based on io-bmp.c
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

#undef DUMPBIH
/*

Icons are just like BMP's, except for the header.
 
Known bugs:
	* bi-tonal files aren't tested 

*/

#include <config.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"
#include <errno.h>



/* 

These structures are actually dummies. These are according to
the "Windows API reference guide volume II" as written by 
Borland International, but GCC fiddles with the alignment of
the internal members.

*/

struct BitmapFileHeader {
	gushort bfType;
	guint bfSize;
	guint reserverd;
	guint bfOffbits;
};

struct BitmapInfoHeader {
	guint biSize;
	guint biWidth;
	guint biHeight;
	gushort biPlanes;
	gushort biBitCount;
	guint biCompression;
	guint biSizeImage;
	guint biXPelsPerMeter;
	guint biYPelsPerMeter;
	guint biClrUsed;
	guint biClrImportant;
};

#ifdef DUMPBIH
/* 

DumpBIH printf's the values in a BitmapInfoHeader to the screen, for 
debugging purposes.

*/
static void DumpBIH(unsigned char *BIH)
{				
	printf("biSize      = %i \n",
	       (int)(BIH[3] << 24) + (BIH[2] << 16) + (BIH[1] << 8) + (BIH[0]));
	printf("biWidth     = %i \n",
	       (int)(BIH[7] << 24) + (BIH[6] << 16) + (BIH[5] << 8) + (BIH[4]));
	printf("biHeight    = %i \n",
	       (int)(BIH[11] << 24) + (BIH[10] << 16) + (BIH[9] << 8) +
	       (BIH[8]));
	printf("biPlanes    = %i \n", (int)(BIH[13] << 8) + (BIH[12]));
	printf("biBitCount  = %i \n", (int)(BIH[15] << 8) + (BIH[14]));
	printf("biCompress  = %i \n",
	       (int)(BIH[19] << 24) + (BIH[18] << 16) + (BIH[17] << 8) +
	       (BIH[16]));
	printf("biSizeImage = %i \n",
	       (int)(BIH[23] << 24) + (BIH[22] << 16) + (BIH[21] << 8) +
	       (BIH[20]));
	printf("biXPels     = %i \n",
	       (int)(BIH[27] << 24) + (BIH[26] << 16) + (BIH[25] << 8) +
	       (BIH[24]));
	printf("biYPels     = %i \n",
	       (int)(BIH[31] << 24) + (BIH[30] << 16) + (BIH[29] << 8) +
	       (BIH[28]));
	printf("biClrUsed   = %i \n",
	       (int)(BIH[35] << 24) + (BIH[34] << 16) + (BIH[33] << 8) +
	       (BIH[32]));
	printf("biClrImprtnt= %i \n",
	       (int)(BIH[39] << 24) + (BIH[38] << 16) + (BIH[37] << 8) +
	       (BIH[36]));
}
#endif

/* Progressive loading */
struct headerpair {
	gint width;
	gint height;
	guint depth;
	guint Negative;		/* Negative = 1 -> top down BMP,  
				   Negative = 0 -> bottom up BMP */
};

struct ico_progressive_state {
	ModulePreparedNotifyFunc prepared_func;
	ModuleUpdatedNotifyFunc updated_func;
	gpointer user_data;

	gint HeaderSize;	/* The size of the header-part (incl colormap) */
	guchar *HeaderBuf;	/* The buffer for the header (incl colormap) */
	gint BytesInHeaderBuf;  /* The size of the allocated HeaderBuf */
	gint HeaderDone;	/* The nr of bytes actually in HeaderBuf */

	gint LineWidth;		/* The width of a line in bytes */
	guchar *LineBuf;	/* Buffer for 1 line */
	gint LineDone;		/* # of bytes in LineBuf */
	gint Lines;		/* # of finished lines */

	gint Type;		/*  
				   24 = RGB
				   8 = 8 bit colormapped
				   1  = 1 bit bitonal 
				 */


	struct headerpair Header;	/* Decoded (BE->CPU) header */
	
	gint			DIBoffset;
	gint			ImageScore;


	GdkPixbuf *pixbuf;	/* Our "target" */
};

static gpointer
gdk_pixbuf__ico_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
				 gpointer user_data,
                                 GError **error);
static gboolean gdk_pixbuf__ico_image_stop_load(gpointer data, GError **error);
static gboolean gdk_pixbuf__ico_image_load_increment(gpointer data,
                                                     const guchar * buf, guint size,
                                                     GError **error);

static void 
context_free (struct ico_progressive_state *context)
{
	if (context->LineBuf != NULL)
		g_free (context->LineBuf);
	context->LineBuf = NULL;
	if (context->HeaderBuf != NULL)
		g_free (context->HeaderBuf);

	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);

	g_free (context);
}
				
/* Shared library entry point --> Can go when generic_image_load
   enters gdk-pixbuf-io */
static GdkPixbuf *
gdk_pixbuf__ico_image_load(FILE * f, GError **error)
{
	guchar membuf [4096];
	size_t length;
	struct ico_progressive_state *State;

	GdkPixbuf *pb;

	State = gdk_pixbuf__ico_image_begin_load(NULL, NULL, NULL, error);

        if (State == NULL)
		return NULL;
        
	while (!feof(f)) {
		length = fread(membuf, 1, 4096, f);
		if (ferror (f)) {
			g_set_error (error,
				     G_FILE_ERROR,
				     g_file_error_from_errno (errno),
				     _("Failure reading ICO: %s"), g_strerror (errno));
			context_free (State);
			return NULL;
		}
		if (length > 0)
                        if (!gdk_pixbuf__ico_image_load_increment(State, membuf, length,
                                                                  error)) {
				context_free (State);
                                return NULL;
			}
	}
	if (State->pixbuf != NULL)
		gdk_pixbuf_ref (State->pixbuf);
	else {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("ICO file was missing some data (perhaps it was truncated somehow?)"));
		context_free (State);
		return NULL;
	}

	pb = State->pixbuf;

	gdk_pixbuf__ico_image_stop_load(State, NULL);
	return pb;
}

static void DecodeHeader(guchar *Data, gint Bytes,
			 struct ico_progressive_state *State,
			 GError **error)
{
/* For ICO's we have to be very clever. There are multiple images possible
   in an .ICO. For now, we select (in order of priority):
     1) The one with the highest number of colors
     2) The largest one
 */   
 
	gint IconCount = 0; /* The number of icon-versions in the file */
	guchar *BIH; /* The DIB for the used icon */
 	guchar *Ptr;
 	gint I;
 
 	/* Step 1: The ICO header */
 
 	IconCount = (Data[5] << 8) + (Data[4]);
 
 	State->HeaderSize = 6 + IconCount*16;
 	
 	if (State->HeaderSize>State->BytesInHeaderBuf) {
 		State->HeaderBuf=g_try_realloc(State->HeaderBuf,State->HeaderSize);
		if (!State->HeaderBuf) {
			g_set_error (error,
				     GDK_PIXBUF_ERROR,
				     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				     _("Not enough memory to load icon"));
			return;
		}
 		State->BytesInHeaderBuf = State->HeaderSize;
 	}
 	if (Bytes < State->HeaderSize)
 		return;
 	
 	/* We now have all the "short-specs" of the versions 
 	   So we iterate through them and select the best one */
 	   
 	State->ImageScore = 0;
 	State->DIBoffset  = 0;
 	Ptr = Data + 6;
	for (I=0;I<IconCount;I++) {
		int ThisWidth, ThisHeight,ThisColors;
		int ThisScore;
		
		ThisWidth = Ptr[0];
		ThisHeight = Ptr[1];
		ThisColors = (Ptr[2]);
		if (ThisColors==0) 
			ThisColors=256; /* Yes, this is in the spec, ugh */
		
		ThisScore = ThisColors*1024+ThisWidth*ThisHeight; 

		if (ThisScore>State->ImageScore) {
			State->ImageScore = ThisScore;
			State->DIBoffset = (Ptr[15]<<24)+(Ptr[14]<<16)+
					   (Ptr[13]<<8) + (Ptr[12]);
								 
		}
		
		
		Ptr += 16;	
	} 
		
	/* We now have a winner, pointed to in State->DIBoffset,
	   so we know how many bytes are in the "header" part. */
	      
	State->HeaderSize = State->DIBoffset + 40; /* 40 = sizeof(InfoHeader) */
	
 	if (State->HeaderSize>State->BytesInHeaderBuf) {
 		State->HeaderBuf=g_try_realloc(State->HeaderBuf,State->HeaderSize);
		if (!State->HeaderBuf) {
			g_set_error (error,
				     GDK_PIXBUF_ERROR,
				     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				     _("Not enough memory to load icon"));
			return;
		}
 		State->BytesInHeaderBuf = State->HeaderSize;
 	}
	if (Bytes<State->HeaderSize) 
		return;   
	
	BIH = Data+State->DIBoffset;

#ifdef DUMPBIH
	DumpBIH(BIH);
#endif	
	
	/* Add the palette to the headersize */
		
	State->Header.width =
	    (int)(BIH[7] << 24) + (BIH[6] << 16) + (BIH[5] << 8) + (BIH[4]);
	if (State->Header.width == 0) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("Icon has zero width"));
		return;
	}
	State->Header.height =
	    (int)(BIH[11] << 24) + (BIH[10] << 16) + (BIH[9] << 8) + (BIH[8])/2;
	    /* /2 because the BIH height includes the transparency mask */
	if (State->Header.height == 0) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("Icon has zero height"));
		return;
	}
	State->Header.depth = (BIH[15] << 8) + (BIH[14]);;

	State->Type = State->Header.depth;	
	if (State->Lines>=State->Header.height)
		State->Type = 1; /* The transparency mask is 1 bpp */
	
	
	
	/* Determine the  palette size. If the header indicates 0, it
	   is actually the maximum for the bpp. You have to love the
	   guys who made the spec. */
	I = (int)(BIH[35] << 24) + (BIH[34] << 16) + (BIH[33] << 8) + (BIH[32]);
	I = I*4;
	if ((I==0)&&(State->Type==1))
		I = 2*4;
	if ((I==0)&&(State->Type==4))
		I = 16*4;
	if ((I==0)&&(State->Type==8))
		I = 256*4;
	
	State->HeaderSize+=I;
	
 	if (State->HeaderSize>State->BytesInHeaderBuf) {
 		State->HeaderBuf=g_try_realloc(State->HeaderBuf,State->HeaderSize);
		if (!State->HeaderBuf) {
			g_set_error (error,
				     GDK_PIXBUF_ERROR,
				     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				     _("Not enough memory to load icon"));
			return;
		}
 		State->BytesInHeaderBuf = State->HeaderSize;
 	}
 	if (Bytes < State->HeaderSize)
 		return;

	if ((BIH[16] != 0) || (BIH[17] != 0) || (BIH[18] != 0)
	    || (BIH[19] != 0)) {
		/* FIXME: is this the correct message? */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Compressed icons are not supported"));
		return;
	}

	/* Negative heights mean top-down pixel-order */
	if (State->Header.height < 0) {
		State->Header.height = -State->Header.height;
		State->Header.Negative = 1;
	}
	if (State->Header.width < 0) {
		State->Header.width = -State->Header.width;
	}
	g_assert (State->Header.width > 0);
	g_assert (State->Header.height > 0);

	if (State->Type == 24)
		State->LineWidth = State->Header.width * 3;
	if (State->Type == 8)
		State->LineWidth = State->Header.width * 1;
	if (State->Type == 4) {
		State->LineWidth = (State->Header.width+1)/2;
	}
	if (State->Type == 1) {
		State->LineWidth = State->Header.width / 8;
		if ((State->Header.width & 7) != 0)
			State->LineWidth++;
	}

	/* Pad to a 32 bit boundary */
	if (((State->LineWidth % 4) > 0))
		State->LineWidth = (State->LineWidth / 4) * 4 + 4;


	if (State->LineBuf == NULL) {
		State->LineBuf = g_try_malloc(State->LineWidth);
		if (!State->LineBuf) {
			g_set_error (error,
				     GDK_PIXBUF_ERROR,
				     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
				     _("Not enough memory to load icon"));
		}
		return;
	}

	g_assert(State->LineBuf != NULL);


	if (State->pixbuf == NULL) {
		State->pixbuf =
		    gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
				   State->Header.width,
				   State->Header.height);

		if (State->prepared_func != NULL)
			/* Notify the client that we are ready to go */
			(*State->prepared_func) (State->pixbuf,
                                                 NULL,
						 State->user_data);

	}

}

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

static gpointer
gdk_pixbuf__ico_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
				 gpointer user_data,
                                 GError **error)
{
	struct ico_progressive_state *context;

	context = g_new0(struct ico_progressive_state, 1);
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;

	context->HeaderSize = 54;
	context->HeaderBuf = g_try_malloc(14 + 40 + 4*256 + 512);
	if (!context->HeaderBuf) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
			     _("Not enough memory to load ICO file"));
		return NULL;
	}
	/* 4*256 for the colormap */
	context->BytesInHeaderBuf = 14 + 40 + 4*256 + 512 ;
	context->HeaderDone = 0;

	context->LineWidth = 0;
	context->LineBuf = NULL;
	context->LineDone = 0;
	context->Lines = 0;

	context->Type = 0;

	memset(&context->Header, 0, sizeof(struct headerpair));


	context->pixbuf = NULL;


	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
gboolean gdk_pixbuf__ico_image_stop_load(gpointer data,
                                         GError **error)
{
	struct ico_progressive_state *context =
	    (struct ico_progressive_state *) data;

        /* FIXME this thing needs to report errors if
         * we have unused image data
         */

	g_return_val_if_fail(context != NULL, TRUE);
	
	context_free (context);
        return TRUE;
}


static void OneLine24(struct ico_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		Pixels[X * 4 + 0] = context->LineBuf[X * 3 + 2];
		Pixels[X * 4 + 1] = context->LineBuf[X * 3 + 1];
		Pixels[X * 4 + 2] = context->LineBuf[X * 3 + 0];
		X++;
	}

}

static void OneLine8(struct ico_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 4 + 0] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 42+context->DIBoffset];
		Pixels[X * 4 + 1] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 41+context->DIBoffset];
		Pixels[X * 4 + 2] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 40+context->DIBoffset];
		X++;
	}
}
static void OneLine4(struct ico_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	
	while (X < context->Header.width) {
		guchar Pix;
		
		Pix = context->LineBuf[X/2];

		Pixels[X * 4 + 0] =
		    context->HeaderBuf[4 * (Pix>>4) + 42+context->DIBoffset];
		Pixels[X * 4 + 1] =
		    context->HeaderBuf[4 * (Pix>>4) + 41+context->DIBoffset];
		Pixels[X * 4 + 2] =
		    context->HeaderBuf[4 * (Pix>>4) + 40+context->DIBoffset];
		X++;
		if (X<context->Header.width) { 
			/* Handle the other 4 bit pixel only when there is one */
			Pixels[X * 4 + 0] =
			    context->HeaderBuf[4 * (Pix&15) + 42+context->DIBoffset];
			Pixels[X * 4 + 1] =
			    context->HeaderBuf[4 * (Pix&15) + 41+context->DIBoffset];
			Pixels[X * 4 + 2] =
			    context->HeaderBuf[4 * (Pix&15) + 40+context->DIBoffset];
			X++;
		}
	}
	
}

static void OneLine1(struct ico_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		int Bit;

		Bit = (context->LineBuf[X / 8]) >> (7 - (X & 7));
		Bit = Bit & 1;
		/* The joys of having a BGR byteorder */
		Pixels[X * 4 + 0] = Bit*255;
		Pixels[X * 4 + 1] = Bit*255;
		Pixels[X * 4 + 2] = Bit*255;
		X++;
	}
}

static void OneLineTransp(struct ico_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (2*context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Lines-context->Header.height));
	while (X < context->Header.width) {
		int Bit;

		Bit = (context->LineBuf[X / 8]) >> (7 - (X & 7));
		Bit = Bit & 1;
		/* The joys of having a BGR byteorder */
		Pixels[X * 4 + 3] = 255-Bit*255;
#if 0		
		if (Bit){
		  Pixels[X*4+0] = 255;
		  Pixels[X*4+1] = 255;
		} else {
		  Pixels[X*4+0] = 0;
		  Pixels[X*4+1] = 0;
		}
#endif		
		X++;
	}
}


static void OneLine(struct ico_progressive_state *context)
{
	context->LineDone = 0;
	
	if (context->Lines >= context->Header.height*2) {
		return;
	}
		
	if (context->Lines <context->Header.height) {		

		if (context->Type == 24)
			OneLine24(context);
		if (context->Type == 8)
			OneLine8(context);
		if (context->Type == 4)
			OneLine4(context);
		if (context->Type == 1)
			OneLine1(context);
	} else
	{
		OneLineTransp(context);
	}
	
	context->Lines++;
	if (context->Lines>=context->Header.height) {
	 	context->Type = 1;
		context->LineWidth = context->Header.width / 8;
		if ((context->Header.width & 7) != 0)
			context->LineWidth++;
		/* Pad to a 32 bit boundary */
		if (((context->LineWidth % 4) > 0))
			context->LineWidth = (context->LineWidth / 4) * 4 + 4;
			
	}
	  

	if (context->updated_func != NULL) {
		(*context->updated_func) (context->pixbuf,
					  0,
					  context->Lines,
					  context->Header.width,
					  context->Header.height,
					  context->user_data);

	}
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
static gboolean
gdk_pixbuf__ico_image_load_increment(gpointer data,
                                     const guchar * buf,
                                     guint size,
                                     GError **error)
{
	struct ico_progressive_state *context =
	    (struct ico_progressive_state *) data;

	gint BytesToCopy;

	while (size > 0) {
		g_assert(context->LineDone >= 0);
		if (context->HeaderDone < context->HeaderSize) {	/* We still 
									   have headerbytes to do */
			BytesToCopy =
			    context->HeaderSize - context->HeaderDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			memmove(context->HeaderBuf + context->HeaderDone,
			       buf, BytesToCopy);

			size -= BytesToCopy;
			buf += BytesToCopy;
			context->HeaderDone += BytesToCopy;
		} 
		else {
			BytesToCopy =
			    context->LineWidth - context->LineDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			if (BytesToCopy > 0) {
				memmove(context->LineBuf +
				       context->LineDone, buf,
				       BytesToCopy);

				size -= BytesToCopy;
				buf += BytesToCopy;
				context->LineDone += BytesToCopy;
			}
			if ((context->LineDone >= context->LineWidth) &&
			    (context->LineWidth > 0))
				OneLine(context);


		}

		if (context->HeaderDone >= 6) {
			GError *decode_err = NULL;
			DecodeHeader(context->HeaderBuf,
				     context->HeaderDone, context, &decode_err);
			if (decode_err) {
				g_propagate_error (error, decode_err);
				return FALSE;
			}
		}
	}

	return TRUE;
}

void
gdk_pixbuf__ico_fill_vtable (GdkPixbufModule *module)
{
  module->load = gdk_pixbuf__ico_image_load;
  module->begin_load = gdk_pixbuf__ico_image_begin_load;
  module->stop_load = gdk_pixbuf__ico_image_stop_load;
  module->load_increment = gdk_pixbuf__ico_image_load_increment;
}
