#include "gdk.h"
#include "gdkprivate-nanox.h"

static void
gdk_nanox_image_destroy (GdkImage *image);

static void
gdk_image_put_normal (GdkImage    *image,
		      GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height);

static GdkImageClass image_class_normal = {
  gdk_nanox_image_destroy,
  gdk_image_put_normal
};

void
gdk_image_exit (void)
{
}


GdkImage *
gdk_image_new_bitmap(GdkVisual *visual, gpointer data, gint w, gint h)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

void
gdk_image_init (void)
{
}


GdkImage*
gdk_image_new (GdkImageType  type,
	       GdkVisual    *visual,
	       gint          width,
	       gint          height)
{
  GdkImage *image;
  GdkImagePrivateX *private;

  private = g_new (GdkImagePrivateX, 1);
  image = (GdkImage*) private;

  private->base.ref_count = 1;
  image->type = type;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = visual->depth;
  
  private->base.klass = &image_class_normal;
  //private->ximage = NULL;
  /* more: implement as a pixmap? */
  return image;
}


GdkImage*
gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}


guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
		g_message("unimplemented %s", __FUNCTION__);
  return 0;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
		g_message("unimplemented %s", __FUNCTION__);
}


static void
gdk_nanox_image_destroy (GdkImage *image)
{
		g_message("unimplemented %s", __FUNCTION__);
}

static void
gdk_image_put_normal (GdkImage    *image,
		      GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
		g_message("unimplemented %s", __FUNCTION__);
}

