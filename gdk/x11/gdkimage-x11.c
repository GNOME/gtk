/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

/* gcc -ansi -pedantic on GNU/Linux causes warnings and errors
 * unless this is defined:
 * warning: #warning "Files using this header must be compiled with _SVID_SOURCE or _XOPEN_SOURCE"
 */
#ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE 1
#endif

#include <stdlib.h>
#include <sys/types.h>

#if defined (HAVE_IPC_H) && defined (HAVE_SHM_H) && defined (HAVE_XSHM_H)
#define USE_SHM
#endif

#ifdef USE_SHM
#include <sys/ipc.h>
#include <sys/shm.h>
#endif /* USE_SHM */

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef USE_SHM
#include <X11/extensions/XShm.h>
#endif /* USE_SHM */

#include <errno.h>

#include "gdk.h"		/* For gdk_error_trap_* / gdk_flush_* */
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkprivate-x11.h"

static GList *image_list = NULL;
static gpointer parent_class = NULL;

static void gdk_x11_image_destroy    (GdkImage      *image);
static void gdk_image_init       (GdkImage      *image);
static void gdk_image_class_init (GdkImageClass *klass);
static void gdk_image_finalize   (GObject       *object);

#define PRIVATE_DATA(image) ((GdkImagePrivateX11 *) GDK_IMAGE (image)->windowing_data)

GType
gdk_image_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkImageClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkImage),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_image_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_image_init (GdkImage *image)
{
  image->windowing_data = g_new0 (GdkImagePrivateX11, 1);
  
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_image_finalize;
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  gdk_x11_image_destroy (image);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}


void
gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_x11_image_destroy (image);
    }
}

GdkImage *
gdk_image_new_bitmap(GdkVisual *visual, gpointer data, gint w, gint h)
/*
 * Desc: create a new bitmap image
 */
{
  Visual *xvisual;
  GdkImage *image;
  GdkImagePrivateX11 *private;
  image = g_object_new (gdk_image_get_type (), NULL);
  private = PRIVATE_DATA (image);
  private->xdisplay = gdk_display;
  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = w;
  image->height = h;
  image->depth = 1;
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;
  private->ximage = XCreateImage(private->xdisplay, xvisual, 1, XYBitmap,
				 0, 0, w ,h, 8, 0);
  private->ximage->data = data;
  private->ximage->bitmap_bit_order = MSBFirst;
  private->ximage->byte_order = MSBFirst;
  image->byte_order = MSBFirst;
  image->mem =  private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bpp = 1;
  return(image);
} /* gdk_image_new_bitmap() */

static int
gdk_image_check_xshm(Display *display)
/* 
 * Desc: query the server for support for the MIT_SHM extension
 * Return:  0 = not available
 *          1 = shared XImage support available
 *          2 = shared Pixmap support available also
 */
{
#ifdef USE_SHM
  int major, minor, ignore;
  Bool pixmaps;
  
  if (XQueryExtension(display, "MIT-SHM", &ignore, &ignore, &ignore)) 
    {
      if (XShmQueryVersion(display, &major, &minor, &pixmaps )==True) 
	{
	  return (pixmaps==True) ? 2 : 1;
	}
    }
#endif /* USE_SHM */
  return 0;
}

void
_gdk_windowing_image_init (void)
{
  if (gdk_use_xshm)
    {
      if (!gdk_image_check_xshm (gdk_display))
	{
	  gdk_use_xshm = False;
	}
    }
}

GdkImage*
gdk_image_new (GdkImageType  type,
	       GdkVisual    *visual,
	       gint          width,
	       gint          height)
{
  GdkImage *image;
  GdkImagePrivateX11 *private;
#ifdef USE_SHM
  XShmSegmentInfo *x_shm_info;
#endif /* USE_SHM */
  Visual *xvisual;

  switch (type)
    {
    case GDK_IMAGE_FASTEST:
      image = gdk_image_new (GDK_IMAGE_SHARED, visual, width, height);

      if (!image)
	image = gdk_image_new (GDK_IMAGE_NORMAL, visual, width, height);
      break;

    default:
      image = g_object_new (gdk_image_get_type (), NULL);
      
      private = PRIVATE_DATA (image);

      private->xdisplay = gdk_display;

      image->type = type;
      image->visual = visual;
      image->width = width;
      image->height = height;
      image->depth = visual->depth;

      xvisual = ((GdkVisualPrivate*) visual)->xvisual;

      switch (type)
	{
	case GDK_IMAGE_SHARED:
#ifdef USE_SHM
	  if (gdk_use_xshm)
	    {
	      private->x_shm_info = g_new (XShmSegmentInfo, 1);
	      x_shm_info = private->x_shm_info;

	      private->ximage = XShmCreateImage (private->xdisplay, xvisual, visual->depth,
						 ZPixmap, NULL, x_shm_info, width, height);
	      if (private->ximage == NULL)
		{
		  g_warning ("XShmCreateImage failed");
		  
		  g_free (image);
		  gdk_use_xshm = False;
		  return NULL;
		}

	      x_shm_info->shmid = shmget (IPC_PRIVATE,
					  private->ximage->bytes_per_line * private->ximage->height,
					  IPC_CREAT | 0777);

	      if (x_shm_info->shmid == -1)
		{
		  /* EINVAL indicates, most likely, that the segment we asked for
		   * is bigger than SHMMAX, so we don't treat it as a permanently
		   * fatal error. ENOSPC and ENOMEM may also indicate this, but
		   * more likely are permanent errors.
		   */
		  if (errno != EINVAL)
		    {
		      g_warning ("shmget failed!");
		      gdk_use_xshm = False;
		    }

		  XDestroyImage (private->ximage);
		  g_free (private->x_shm_info);
		  g_free (image);

		  return NULL;
		}

	      x_shm_info->readOnly = False;
	      x_shm_info->shmaddr = shmat (x_shm_info->shmid, 0, 0);
	      private->ximage->data = x_shm_info->shmaddr;

	      if (x_shm_info->shmaddr == (char*) -1)
		{
		  g_warning ("shmat failed!");

		  XDestroyImage (private->ximage);
		  shmctl (x_shm_info->shmid, IPC_RMID, 0);
		  
		  g_free (private->x_shm_info);
		  g_free (image);

		  return NULL;
		}

	      gdk_error_trap_push ();

	      XShmAttach (private->xdisplay, x_shm_info);
	      XSync (private->xdisplay, False);

	      if (gdk_error_trap_pop ())
		{
		  /* this is the common failure case so omit warning */
		  XDestroyImage (private->ximage);
		  shmdt (x_shm_info->shmaddr);
		  shmctl (x_shm_info->shmid, IPC_RMID, 0);
                  
		  g_free (private->x_shm_info);
		  g_free (image);

		  gdk_use_xshm = False;

		  return NULL;
		}
	      
	      /* We mark the segment as destroyed so that when
	       * the last process detaches, it will be deleted.
	       * There is a small possibility of leaking if
	       * we die in XShmAttach. In theory, a signal handler
	       * could be set up.
	       */
	      shmctl (x_shm_info->shmid, IPC_RMID, 0);		      

	      if (image)
		image_list = g_list_prepend (image_list, image);
	    }
	  else
	    {
	      g_free (image);
	      return NULL;
	    }
	  break;
#else /* USE_SHM */
	  g_free (image);
	  return NULL;
#endif /* USE_SHM */
	case GDK_IMAGE_NORMAL:
	  private->ximage = XCreateImage (private->xdisplay, xvisual, visual->depth,
					  ZPixmap, 0, 0, width, height, 32, 0);

	  /* Use malloc, not g_malloc here, because X will call free()
	   * on this data
	   */
	  private->ximage->data = malloc (private->ximage->bytes_per_line *
					  private->ximage->height);
	  break;

	case GDK_IMAGE_FASTEST:
	  g_assert_not_reached ();
	}

      if (image)
	{
	  image->byte_order = private->ximage->byte_order;
	  image->mem = private->ximage->data;
	  image->bpl = private->ximage->bytes_per_line;
	  image->bpp = (private->ximage->bits_per_pixel + 7) / 8;
	}
    }

  return image;
}

GdkImage*
_gdk_x11_get_image (GdkDrawable    *drawable,
                    gint            x,
                    gint            y,
                    gint            width,
                    gint            height)
{
  GdkImage *image;
  GdkImagePrivateX11 *private;
  GdkDrawableImplX11 *impl;
  GdkVisual *visual;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_X11 (drawable), NULL);

  visual = gdk_drawable_get_visual (drawable);

  if (visual == NULL)
    {
      g_warning ("To get the image from a drawable, the drawable "
                 "must have a visual and colormap; calling "
                 "gtk_drawable_set_colormap() on a drawable "
                 "created without a colormap should solve this problem");

      return NULL;
    }
  
  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  image = g_object_new (gdk_image_get_type (), NULL);
  private = PRIVATE_DATA (image);

  private->xdisplay = gdk_display;
  private->ximage = XGetImage (private->xdisplay,
                               impl->xid,
			       x, y, width, height,
			       AllPlanes, ZPixmap);

  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = private->ximage->depth;

  image->mem = private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  if (private->ximage->bits_per_pixel <= 8)
    image->bpp = 1;
  else if (private->ximage->bits_per_pixel <= 16)
    image->bpp = 2;
  else if (private->ximage->bits_per_pixel <= 24)
    image->bpp = 3;
  else
    image->bpp = 4;
  image->byte_order = private->ximage->byte_order;

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  guint32 pixel;
  GdkImagePrivateX11 *private;

  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  private = PRIVATE_DATA (image);

  pixel = XGetPixel (private->ximage, x, y);

  return pixel;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  GdkImagePrivateX11 *private;

  g_return_if_fail (GDK_IS_IMAGE (image));

  private = PRIVATE_DATA (image);

  pixel = XPutPixel (private->ximage, x, y, pixel);
}

static void
gdk_x11_image_destroy (GdkImage *image)
{
  GdkImagePrivateX11 *private;
#ifdef USE_SHM
  XShmSegmentInfo *x_shm_info;
#endif /* USE_SHM */

  g_return_if_fail (GDK_IS_IMAGE (image));

  private = PRIVATE_DATA (image);

  if (private == NULL) /* This means that gdk_image_exit() destroyed the
                        * image already, and now we're called a second
                        * time from _finalize()
                        */
    return;
  
  switch (image->type)
    {
    case GDK_IMAGE_NORMAL:
      XDestroyImage (private->ximage);
      break;

    case GDK_IMAGE_SHARED:
#ifdef USE_SHM
      gdk_flush();

      XShmDetach (private->xdisplay, private->x_shm_info);
      XDestroyImage (private->ximage);

      x_shm_info = private->x_shm_info;
      shmdt (x_shm_info->shmaddr);
      
      g_free (private->x_shm_info);
      private->x_shm_info = NULL;
      
      image_list = g_list_remove (image_list, image);
#else /* USE_SHM */
      g_error ("trying to destroy shared memory image when gdk was compiled without shared memory support");
#endif /* USE_SHM */
      break;

    case GDK_IMAGE_FASTEST:
      g_assert_not_reached ();
    }

  g_free (private);
  image->windowing_data = NULL;
}
