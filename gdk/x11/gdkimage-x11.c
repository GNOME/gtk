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

static void gdk_x11_image_destroy (GdkImage      *image);
static void gdk_image_init        (GdkImage      *image);
static void gdk_image_class_init  (GdkImageClass *klass);
static void gdk_image_finalize    (GObject       *object);

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
  image->bits_per_pixel = 1;
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
	      x_shm_info->shmid = -1;
	      x_shm_info->shmaddr = (char*) -1;

	      private->ximage = XShmCreateImage (private->xdisplay, xvisual, visual->depth,
						 ZPixmap, NULL, x_shm_info, width, height);
	      if (private->ximage == NULL)
		{
		  g_warning ("XShmCreateImage failed");
		  gdk_use_xshm = FALSE;

		  goto error;
		}

	      x_shm_info->shmid = shmget (IPC_PRIVATE,
					  private->ximage->bytes_per_line * private->ximage->height,
					  IPC_CREAT | 0600);

	      if (x_shm_info->shmid == -1)
		{
		  /* EINVAL indicates, most likely, that the segment we asked for
		   * is bigger than SHMMAX, so we don't treat it as a permanent
		   * error. ENOSPC and ENOMEM may also indicate this, but
		   * more likely are permanent errors.
		   */
		  if (errno != EINVAL)
		    {
		      g_warning ("shmget failed: error %d (%s)", errno, g_strerror (errno));
		      gdk_use_xshm = FALSE;
		    }

		  goto error;
		}

	      x_shm_info->readOnly = False;
	      x_shm_info->shmaddr = shmat (x_shm_info->shmid, 0, 0);
	      private->ximage->data = x_shm_info->shmaddr;

	      if (x_shm_info->shmaddr == (char*) -1)
		{
		  g_warning ("shmat failed: error %d (%s)", errno, g_strerror (errno));
		  /* Failure in shmat is almost certainly permanent. Most likely error is
		   * EMFILE, which would mean that we've exceeded the per-process
		   * Shm segment limit.
		   */
		  gdk_use_xshm = FALSE;
		  goto error;
		}

	      gdk_error_trap_push ();

	      XShmAttach (private->xdisplay, x_shm_info);
	      XSync (private->xdisplay, False);

	      if (gdk_error_trap_pop ())
		{
		  /* this is the common failure case so omit warning */
		  gdk_use_xshm = FALSE;
		  goto error;
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
#endif /* USE_SHM */
	    goto error;
	  break;
	case GDK_IMAGE_NORMAL:
	  private->ximage = XCreateImage (private->xdisplay, xvisual, visual->depth,
					  ZPixmap, 0, 0, width, height, 32, 0);

	  /* Use malloc, not g_malloc here, because X will call free()
	   * on this data
	   */
	  private->ximage->data = malloc (private->ximage->bytes_per_line *
					  private->ximage->height);
	  if (!private->ximage->data)
	    goto error;
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
	  image->bits_per_pixel = private->ximage->bits_per_pixel;
	}
    }

  return image;

 error:
  if (private->ximage)
    {
      XDestroyImage (private->ximage);
      private->ximage = NULL;
    }
#ifdef USE_SHM
  if (private->x_shm_info)
    {
      x_shm_info = private->x_shm_info;
      
      if (x_shm_info->shmaddr != (char *)-1)
	shmdt (x_shm_info->shmaddr);
      if (x_shm_info->shmid != -1) 
	shmctl (x_shm_info->shmid, IPC_RMID, 0);
      
      g_free (x_shm_info);
      private->x_shm_info = NULL;
    }
#endif /* USE_SHM */
  g_object_unref (image);
  
  return NULL;
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
  gboolean have_grab;
  GdkRectangle req;
  GdkRectangle window_rect;
  XImage *ximage;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_X11 (drawable), NULL);

  visual = gdk_drawable_get_visual (drawable);

  g_assert (visual || !GDK_IS_WINDOW_IMPL_X11 (drawable));

  impl = GDK_DRAWABLE_IMPL_X11 (drawable);
  
  have_grab = FALSE;

  if (GDK_IS_WINDOW_IMPL_X11 (drawable))
    {
      GdkRectangle screen_rect;
      Window child;

      g_assert (visual);

      have_grab = TRUE;
      gdk_x11_grab_server ();

      /* Translate screen area into window coordinates */
      XTranslateCoordinates (gdk_display,
			     gdk_root_window,
                             impl->xid,
			     0, 0, 
			     &screen_rect.x, &screen_rect.y, 
			     &child);

      screen_rect.width = gdk_screen_width ();
      screen_rect.height = gdk_screen_height ();
      
      gdk_error_trap_push ();

      window_rect.x = 0;
      window_rect.y = 0;
      
      gdk_window_get_geometry (GDK_WINDOW (impl->wrapper),
                               NULL, NULL,
                               &window_rect.width,
                               &window_rect.height,
                               NULL);
      
      /* compute intersection of screen and window, in window
       * coordinates
       */
      if (gdk_error_trap_pop () ||
          !gdk_rectangle_intersect (&window_rect, &screen_rect, 
                                    &window_rect))
        {
          if (have_grab)
            gdk_x11_ungrab_server ();
          return image = gdk_image_new (GDK_IMAGE_FASTEST,
                                        visual,
                                        width, height);
        }
    }
  else
    {
      window_rect.x = 0;
      window_rect.y = 0;
      gdk_drawable_get_size (drawable,
			     &window_rect.width,
			     &window_rect.height);
    }
      
  req.x = x;
  req.y = y;
  req.width = width;
  req.height = height;
  
  /* window_rect specifies the part of drawable which we can get from
   * the server in window coordinates. 
   * For pixmaps this is all of the pixmap, for windows it is just 
   * the onscreen part.
   */
  if (!gdk_rectangle_intersect (&req, &window_rect, &req) && visual) 
    {      
      if (have_grab)
	gdk_x11_ungrab_server ();
      return image = gdk_image_new (GDK_IMAGE_FASTEST,
                                    visual,
                                    width, height);
    }

  if (req.x != x || req.y != y)
    {
      g_assert (GDK_IS_WINDOW (drawable));
      g_assert (visual);

      image = gdk_image_new (GDK_IMAGE_FASTEST,
                             visual,
                             width, height);
      if (image == NULL)
        {
          if (have_grab)
            gdk_x11_ungrab_server ();
          return NULL;
        }

      private = PRIVATE_DATA (image);

      gdk_error_trap_push ();

      ximage = XGetSubImage (impl->xdisplay,
                             impl->xid,
                             req.x, req.y, req.width, req.height,
                             AllPlanes, ZPixmap,
                             private->ximage,
                             req.x - x, req.y - y);

      if (have_grab)
        {
          gdk_x11_ungrab_server ();
          have_grab = FALSE;
        }
      
      if (gdk_error_trap_pop () || ximage == NULL)
        {
          g_object_unref (G_OBJECT (image));
          return NULL;
        }

      g_assert (ximage == private->ximage);
    }
  else
    {
      /* Here we ignore the req.width, req.height -
       * XGetImage() will do the right thing without
       * them.
       */
      ximage = XGetImage (impl->xdisplay,
                          impl->xid,
                          x, y, width, height,
                          AllPlanes, ZPixmap);

      if (have_grab)
        {
          gdk_x11_ungrab_server ();
          have_grab = FALSE;
        }      

      if (!ximage)
        return NULL;

      image = g_object_new (gdk_image_get_type (), NULL);

      private = PRIVATE_DATA (image);

      private->xdisplay = gdk_display;
      private->ximage = ximage;

      image->type = GDK_IMAGE_NORMAL;
      image->visual = visual; /* May be NULL */
      image->width = width;
      image->height = height;
      image->depth = gdk_drawable_get_depth (drawable);

      image->mem = private->ximage->data;
      image->bpl = private->ximage->bytes_per_line;
      image->bits_per_pixel = private->ximage->bits_per_pixel;
      image->bpp = (private->ximage->bits_per_pixel + 7) / 8;
      image->byte_order = private->ximage->byte_order; 
    }

  g_assert (!have_grab);
  
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

  if (private->ximage)		/* Deal with failure of creation */
    {
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
    }

  g_free (private);
  image->windowing_data = NULL;
}
