#include "gdk.h"
#include "gdkprivate-nanox.h"

GdkDrawableClass _gdk_nanox_pixmap_class;

static void
gdk_nanox_pixmap_destroy (GdkPixmap *pixmap)
{
  GrDestroyWindow (GDK_DRAWABLE_XID (pixmap));
  gdk_xid_table_remove (GDK_DRAWABLE_XID (pixmap));

  g_free (GDK_DRAWABLE_XDATA (pixmap));
}

static GdkDrawable *
gdk_nanox_pixmap_alloc (void)
{
  GdkDrawable *drawable;
  GdkDrawablePrivate *private;
  
  static GdkDrawableClass klass;
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;
      
      klass = _gdk_nanox_drawable_class;
      klass.destroy = gdk_nanox_pixmap_destroy;
    }

  drawable = gdk_drawable_alloc ();
  private = (GdkDrawablePrivate *)drawable;

  private->klass = &klass;
  private->klass_data = g_new (GdkDrawableXData, 1);
  private->window_type = GDK_DRAWABLE_PIXMAP;

  return drawable;
}

GdkPixmap*
gdk_pixmap_new (GdkWindow *window,
		gint       width,
		gint       height,
		gint       depth)
{
  GdkPixmap *pixmap;
  GdkDrawablePrivate *private;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  g_return_val_if_fail ((window != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  
  if (!window)
    window = gdk_parent_root;

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;

  if (depth == -1)
    depth = gdk_drawable_get_visual (window)->depth;

  pixmap = gdk_nanox_pixmap_alloc ();
  private = (GdkDrawablePrivate *)pixmap;

  GDK_DRAWABLE_XDATA (private)->xid = GrNewPixmap (width, height, NULL);
  private->width = width;
  private->height = height;

  gdk_xid_table_insert (&GDK_DRAWABLE_XID (pixmap), pixmap);

  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkWindow   *window,
			     const gchar *data,
			     gint         width,
			     gint         height,
			     gint         depth,
			     GdkColor    *fg,
			     GdkColor    *bg)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}


GdkPixmap*
gdk_pixmap_colormap_create_from_xpm (GdkWindow   *window,
				     GdkColormap *colormap,
				     GdkBitmap  **mask,
				     GdkColor    *transparent_color,
				     const gchar *filename)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}


GdkPixmap*
gdk_pixmap_create_from_xpm (GdkWindow  *window,
			    GdkBitmap **mask,
			    GdkColor   *transparent_color,
			    const gchar *filename)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

GdkPixmap*
gdk_pixmap_colormap_create_from_xpm_d (GdkWindow  *window,
				       GdkColormap *colormap,
				       GdkBitmap **mask,
				       GdkColor   *transparent_color,
				       gchar     **data)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

GdkPixmap*
gdk_pixmap_create_from_xpm_d (GdkWindow  *window,
			      GdkBitmap **mask,
			      GdkColor   *transparent_color,
			      gchar     **data)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

GdkPixmap*
gdk_pixmap_foreign_new (guint32 anid)
{
		g_message("unimplemented %s", __FUNCTION__);
  return NULL;
}

