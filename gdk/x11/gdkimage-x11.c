/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "../config.h"

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

#include "gdk.h"
#include "gdkprivate.h"


static void gdk_image_put_normal (GdkDrawable *drawable,
				  GdkGC       *gc,
				  GdkImage    *image,
				  gint         xsrc,
				  gint         ysrc,
				  gint         xdest,
				  gint         ydest,
				  gint         width,
				  gint         height);
static void gdk_image_put_shared (GdkDrawable *drawable,
				  GdkGC       *gc,
				  GdkImage    *image,
				  gint         xsrc,
				  gint         ysrc,
				  gint         xdest,
				  gint         ydest,
				  gint         width,
				  gint         height);


static GList *image_list = NULL;


void
gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_image_destroy (image);
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
        GdkImagePrivate *private;
        private = g_new(GdkImagePrivate, 1);
        image = (GdkImage *) private;
        private->xdisplay = gdk_display;
        private->image_put = gdk_image_put_normal;
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
gdk_image_init (void)
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
  GdkImagePrivate *private;
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
      private = g_new (GdkImagePrivate, 1);
      image = (GdkImage*) private;

      private->xdisplay = gdk_display;
      private->image_put = NULL;

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
	      private->image_put = gdk_image_put_shared;

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
		  g_warning ("shmget failed!");

		  XDestroyImage (private->ximage);
		  g_free (private->x_shm_info);
		  g_free (image);

		  gdk_use_xshm = False;
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

#ifdef	IPC_RMID_DEFERRED_RELEASE
	      if (x_shm_info->shmaddr != (char*) -1)
		shmctl (x_shm_info->shmid, IPC_RMID, 0);		      
#endif

	      gdk_error_code = 0;
	      gdk_error_warnings = 0;

	      XShmAttach (private->xdisplay, x_shm_info);
	      XSync (private->xdisplay, False);

	      gdk_error_warnings = 1;
	      if (gdk_error_code == -1)
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
	  private->image_put = gdk_image_put_normal;

	  private->ximage = XCreateImage (private->xdisplay, xvisual, visual->depth,
					  ZPixmap, 0, 0, width, height, 32, 0);

	  private->ximage->data = g_new (char, private->ximage->bytes_per_line *
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

	  switch (private->ximage->bits_per_pixel)
	    {
	    case 8:
	      image->bpp = 1;
	      break;
	    case 16:
	      image->bpp = 2;
	      break;
	    case 24:
	      image->bpp = 3;
	      break;
	    case 32:
	      image->bpp = 4;
	      break;
	    }
	}
    }

  return image;
}

GdkImage*
gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  GdkImage *image;
  GdkImagePrivate *private;
  GdkWindowPrivate *win_private;

  g_return_val_if_fail (window != NULL, NULL);

  win_private = (GdkWindowPrivate *) window;
  if (win_private->destroyed)
    return NULL;

  private = g_new (GdkImagePrivate, 1);
  image = (GdkImage*) private;

  private->xdisplay = gdk_display;
  private->image_put = gdk_image_put_normal;
  private->ximage = XGetImage (private->xdisplay,
			       win_private->xwindow,
			       x, y, width, height,
			       AllPlanes, ZPixmap);

  image->type = GDK_IMAGE_NORMAL;
  image->visual = gdk_window_get_visual (window);
  image->width = width;
  image->height = height;
  image->depth = private->ximage->depth;

  image->mem = private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bpp = 1;

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  guint32 pixel;
  GdkImagePrivate *private;

  g_return_val_if_fail (image != NULL, 0);

  private = (GdkImagePrivate *) image;

  pixel = XGetPixel (private->ximage, x, y);

  return pixel;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  GdkImagePrivate *private;

  g_return_if_fail (image != NULL);

  private = (GdkImagePrivate *) image;

  pixel = XPutPixel (private->ximage, x, y, pixel);
}

void
gdk_image_destroy (GdkImage *image)
{
  GdkImagePrivate *private;
#ifdef USE_SHM
  XShmSegmentInfo *x_shm_info;
#endif /* USE_SHM */

  g_return_if_fail (image != NULL);

  private = (GdkImagePrivate*) image;
  switch (image->type)
    {
    case GDK_IMAGE_NORMAL:
      XDestroyImage (private->ximage);
      break;

    case GDK_IMAGE_SHARED:
#ifdef USE_SHM
      XShmDetach (private->xdisplay, private->x_shm_info);
      XDestroyImage (private->ximage);

      x_shm_info = private->x_shm_info;
      shmdt (x_shm_info->shmaddr);
      shmctl (x_shm_info->shmid, IPC_RMID, 0);
      
      g_free (private->x_shm_info);

      image_list = g_list_remove (image_list, image);
#else /* USE_SHM */
      g_error ("trying to destroy shared memory image when gdk was compiled without shared memory support");
#endif /* USE_SHM */
      break;

    case GDK_IMAGE_FASTEST:
      g_assert_not_reached ();
    }

  g_free (image);
}

static void
gdk_image_put_normal (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkImage    *image,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
  GdkWindowPrivate *drawable_private;
  GdkImagePrivate *image_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  image_private = (GdkImagePrivate*) image;
  gc_private = (GdkGCPrivate*) gc;

  g_return_if_fail (image->type == GDK_IMAGE_NORMAL);

  XPutImage (drawable_private->xdisplay, drawable_private->xwindow,
	     gc_private->xgc, image_private->ximage,
	     xsrc, ysrc, xdest, ydest, width, height);
}

static void
gdk_image_put_shared (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkImage    *image,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
#ifdef USE_SHM
  GdkWindowPrivate *drawable_private;
  GdkImagePrivate *image_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  image_private = (GdkImagePrivate*) image;
  gc_private = (GdkGCPrivate*) gc;

  g_return_if_fail (image->type == GDK_IMAGE_SHARED);

  XShmPutImage (drawable_private->xdisplay, drawable_private->xwindow,
		gc_private->xgc, image_private->ximage,
		xsrc, ysrc, xdest, ydest, width, height, False);
#else /* USE_SHM */
  g_error ("trying to draw shared memory image when gdk was compiled without shared memory support");
#endif /* USE_SHM */
}
