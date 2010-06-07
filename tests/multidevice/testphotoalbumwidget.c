/*
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include "testphotoalbumwidget.h"
#include <math.h>

#define TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), TEST_TYPE_PHOTO_ALBUM_WIDGET, TestPhotoAlbumWidgetPrivate))

typedef struct TestPhotoAlbumWidgetPrivate TestPhotoAlbumWidgetPrivate;
typedef struct TestPhoto TestPhoto;

struct TestPhoto
{
  GtkDeviceGroup *group;
  gdouble center_x;
  gdouble center_y;
  gdouble x;
  gdouble y;
  gdouble angle;
  gdouble zoom;

  cairo_surface_t *surface;

  GdkPoint points[4];

  gdouble base_zoom;
  gdouble base_angle;
  gdouble initial_distance;
  gdouble initial_angle;
};

struct TestPhotoAlbumWidgetPrivate
{
  GPtrArray *photos;
};

static GQuark quark_group_photo = 0;


static void test_photo_album_widget_class_init (TestPhotoAlbumWidgetClass *klass);
static void test_photo_album_widget_init       (TestPhotoAlbumWidget      *album);

static void test_photo_album_widget_destroy    (GtkObject          *object);

static gboolean test_photo_album_widget_button_press      (GtkWidget           *widget,
                                                           GdkEventButton      *event);
static gboolean test_photo_album_widget_button_release    (GtkWidget           *widget,
                                                           GdkEventButton      *event);
static void     test_photo_album_widget_multidevice_event (GtkWidget           *widget,
                                                           GtkDeviceGroup      *group,
                                                           GtkMultiDeviceEvent *event);
static gboolean test_photo_album_widget_expose            (GtkWidget           *widget,
                                                           GdkEventExpose      *event);

G_DEFINE_TYPE (TestPhotoAlbumWidget, test_photo_album_widget, GTK_TYPE_DRAWING_AREA)


static void
test_photo_album_widget_class_init (TestPhotoAlbumWidgetClass *klass)
{
  GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->destroy = test_photo_album_widget_destroy;

  widget_class->button_press_event = test_photo_album_widget_button_press;
  widget_class->button_release_event = test_photo_album_widget_button_release;
  widget_class->expose_event = test_photo_album_widget_expose;

  g_type_class_add_private (klass, sizeof (TestPhotoAlbumWidgetPrivate));

  quark_group_photo = g_quark_from_static_string ("group-photo");
}

static void
test_photo_album_widget_init (TestPhotoAlbumWidget *album)
{
  TestPhotoAlbumWidgetPrivate *priv;
  GtkWidget *widget;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);
  widget = GTK_WIDGET (album);

  priv->photos = g_ptr_array_new ();

  gtk_widget_add_events (widget,
                         (GDK_POINTER_MOTION_MASK |
                          GDK_BUTTON_MOTION_MASK |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK));

  gtk_widget_set_support_multidevice (widget, TRUE);

  /* Multidevice events are not exposed through GtkWidgetClass */
  g_signal_connect (album, "multidevice-event",
                    G_CALLBACK (test_photo_album_widget_multidevice_event), NULL);
}

static void
calculate_rotated_point (gdouble  angle,
                         gdouble  zoom,
                         gdouble  center_x,
                         gdouble  center_y,
                         gdouble  point_x,
                         gdouble  point_y,
                         gdouble *ret_x,
                         gdouble *ret_y)
{
  gdouble distance, xd, yd, ang;

  if (angle == 0)
    {
      *ret_x = point_x;
      *ret_y = point_y;
      return;
    }

  xd = center_x - point_x;
  yd = center_y - point_y;

  if (xd == 0 && yd == 0)
    {
      *ret_x = center_x;
      *ret_y = center_y;
      return;
    }

  distance = sqrt ((xd * xd) + (yd * yd));
  distance *= zoom;

  ang = atan2 (xd, yd);

  /* Invert angle */
  ang = (2 * G_PI) - ang;

  /* Shift it 270° */
  ang += 3 * (G_PI / 2);

  /* And constraint it to 0°-360° */
  ang = fmod (ang, 2 * G_PI);
  ang += angle;

  *ret_x = center_x + (distance * cos (ang));
  *ret_y = center_y + (distance * sin (ang));
}

static void
test_photo_bounding_rect (TestPhoto    *photo,
                          GdkRectangle *rect)
{
  gint i, left, right, top, bottom;

  left = top = G_MAXINT;
  right = bottom = 0;

  for (i = 0; i < 4; i++)
    {
      if (photo->points[i].x < left)
        left = photo->points[i].x;

      if (photo->points[i].x > right)
        right = photo->points[i].x;

      if (photo->points[i].y < top)
        top = photo->points[i].y;

      if (photo->points[i].y > bottom)
        bottom = photo->points[i].y;
    }

  rect->x = left - 20;
  rect->y = top - 20;
  rect->width = right - left + 40;
  rect->height = bottom - top + 40;
}

static void
allocate_photo_region (TestPhoto *photo)
{
  gint width, height, i;

  width = cairo_image_surface_get_width (photo->surface);
  height = cairo_image_surface_get_height (photo->surface);

  /* Top/left */
  photo->points[0].x = photo->x - photo->center_x;
  photo->points[0].y = photo->y - photo->center_y;

  /* Top/right */
  photo->points[1].x = photo->x - photo->center_x + width;
  photo->points[1].y = photo->y - photo->center_y;

  /* Bottom/right */
  photo->points[2].x = photo->x - photo->center_x + width;
  photo->points[2].y = photo->y - photo->center_y + height;

  /* Bottom/left */
  photo->points[3].x = photo->x - photo->center_x;
  photo->points[3].y = photo->y - photo->center_y + height;

  for (i = 0; i < 4; i++)
    {
      gdouble ret_x, ret_y;

      calculate_rotated_point (photo->angle,
                               photo->zoom,
                               photo->x,
                               photo->y,
                               (gdouble) photo->points[i].x,
                               (gdouble) photo->points[i].y,
                               &ret_x,
                               &ret_y);

      photo->points[i].x = (gint) ret_x;
      photo->points[i].y = (gint) ret_y;
    }
}

static TestPhoto *
test_photo_new (TestPhotoAlbumWidget *album,
                GdkPixbuf            *pixbuf)
{
  TestPhoto *photo;
  static gdouble angle = 0;
  gint width, height;
  cairo_t *cr;

  photo = g_slice_new0 (TestPhoto);
  photo->group = gtk_widget_create_device_group (GTK_WIDGET (album));
  g_object_set_qdata (G_OBJECT (photo->group), quark_group_photo, photo);

  photo->center_x = 0;
  photo->center_y = 0;
  photo->x = 0;
  photo->y = 0;
  photo->angle = angle;
  photo->zoom = 1.0;

  angle += 0.2;

  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  /* Put the pixbuf into an image surface */
  photo->surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  cr = cairo_create (photo->surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0., 0., 0.);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_stroke (cr);

  allocate_photo_region (photo);

  return photo;
}

static void
test_photo_free (TestPhoto            *photo,
                 TestPhotoAlbumWidget *album)
{
  gtk_widget_remove_device_group (GTK_WIDGET (album), photo->group);
  cairo_surface_destroy (photo->surface);

  g_slice_free (TestPhoto, photo);
}

static void
test_photo_raise (TestPhoto            *photo,
                  TestPhotoAlbumWidget *album)
{
  TestPhotoAlbumWidgetPrivate *priv;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);
  g_ptr_array_remove (priv->photos, photo);
  g_ptr_array_add (priv->photos, photo);
}

static gboolean
test_photo_point_in (TestPhoto *photo,
                     gint       x,
                     gint       y)
{
  GdkPoint *left, *right, *top, *bottom;
  gint i;

  left = right = top = bottom = NULL;

  for (i = 0; i < 4; i++)
    {
      GdkPoint *p = &photo->points[i];

      if (!left ||
          p->x < left->x ||
          (p->x == left->x && p->y > left->y))
        left = p;

      if (!right ||
          p->x > right->x ||
          (p->x == right->x && p->y < right->y))
        right = p;
    }

  for (i = 0; i < 4; i++)
    {
      GdkPoint *p = &photo->points[i];

      if (p == left || p == right)
        continue;

      if (!top ||
          p->y < top->y)
        top = p;

      if (!bottom ||
          p->y > bottom->y)
        bottom = p;
    }

  g_assert (left && right && top && bottom);

  if (x < left->x ||
      x > right->x ||
      y < top->y ||
      y > bottom->y)
    return FALSE;

  /* Check whether point is above the sides
   * between leftmost and topmost, and
   * topmost and rightmost corners.
   */
  if (x <= top->x)
    {
      if (left->y - ((left->y - top->y) * (((gdouble) x - left->x) / (top->x - left->x))) > y)
        return FALSE;
    }
  else
    {
      if (top->y + ((right->y - top->y) * (((gdouble) x - top->x) / (right->x - top->x))) > y)
        return FALSE;
    }

  /* Check whether point is below the sides
   * between leftmost and bottom, and
   * bottom and rightmost corners.
   */
  if (x <= bottom->x)
    {
      if (left->y + ((bottom->y - left->y) * (((gdouble) x - left->x) / (bottom->x - left->x))) < y)
        return FALSE;
    }
  else
    {
      if (bottom->y - ((bottom->y - right->y) * (((gdouble) x - bottom->x) / (right->x - bottom->x))) < y)
        return FALSE;
    }

  return TRUE;
}

static void
test_photo_album_widget_destroy (GtkObject *object)
{
  TestPhotoAlbumWidgetPrivate *priv;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (object);

  if (priv->photos)
    {
      g_ptr_array_foreach (priv->photos, (GFunc) test_photo_free, object);
      g_ptr_array_free (priv->photos, TRUE);
      priv->photos = NULL;
    }

  GTK_OBJECT_CLASS (test_photo_album_widget_parent_class)->destroy (object);
}

static TestPhoto *
find_photo_at_position (TestPhotoAlbumWidget *album,
                        gdouble               x,
                        gdouble               y)
{
  TestPhotoAlbumWidgetPrivate *priv;
  TestPhoto *photo;
  gint i;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);

  for (i = priv->photos->len - 1; i >= 0; i--)
    {
      photo = g_ptr_array_index (priv->photos, i);

      if (test_photo_point_in (photo, (gint) x, (gint) y))
        return photo;
    }

  return NULL;
}

static gboolean
test_photo_album_widget_button_press (GtkWidget      *widget,
                                      GdkEventButton *event)
{
  TestPhoto *photo;

  photo = find_photo_at_position (TEST_PHOTO_ALBUM_WIDGET (widget), event->x, event->y);

  if (!photo)
    return FALSE;

  test_photo_raise (photo, TEST_PHOTO_ALBUM_WIDGET (widget));
  gtk_device_group_add_device (photo->group, event->device);

  return TRUE;
}

static gboolean
test_photo_album_widget_button_release (GtkWidget      *widget,
                                        GdkEventButton *event)
{
  GtkDeviceGroup *group;

  group = gtk_widget_get_group_for_device (widget, event->device);

  if (group)
    gtk_device_group_remove_device (group, event->device);

  return TRUE;
}

static void
test_photo_album_widget_multidevice_event (GtkWidget           *widget,
                                           GtkDeviceGroup      *group,
                                           GtkMultiDeviceEvent *event)
{
  TestPhotoAlbumWidgetPrivate *priv;
  cairo_region_t *region;
  TestPhoto *photo;
  gboolean new_center = FALSE;
  gboolean new_position = FALSE;
  gdouble event_x, event_y;
  GdkRectangle rect;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (widget);
  photo = g_object_get_qdata (G_OBJECT (group), quark_group_photo);

  test_photo_bounding_rect (photo, &rect);
  region = cairo_region_create_rectangle ((cairo_rectangle_int_t *) &rect);

  if (event->n_events == 1)
    {
      if (event->type == GTK_EVENT_DEVICE_REMOVED)
        {
          /* Device was just removed, unset zoom/angle info */
          photo->base_zoom = 0;
          photo->base_angle = 0;
          photo->initial_angle = 0;
          photo->initial_distance = 0;
          new_center = TRUE;
        }
      else if (event->type == GTK_EVENT_DEVICE_ADDED)
        new_center = TRUE;

      event_x = event->events[0]->x;
      event_y = event->events[0]->y;
      new_position = TRUE;
    }
  else if (event->n_events == 2)
    {
      gdouble distance, angle;

      gdk_events_get_center ((GdkEvent *) event->events[0],
                             (GdkEvent *) event->events[1],
                             &event_x, &event_y);

      gdk_events_get_distance ((GdkEvent *) event->events[0],
                               (GdkEvent *) event->events[1],
                               &distance);

      gdk_events_get_angle ((GdkEvent *) event->events[0],
                            (GdkEvent *) event->events[1],
                            &angle);

      if (event->type == GTK_EVENT_DEVICE_ADDED)
        {
          photo->base_zoom = photo->zoom;
          photo->base_angle = photo->angle;
          photo->initial_angle = angle;
          photo->initial_distance = distance;
          new_center = TRUE;
        }

      photo->zoom = photo->base_zoom * (distance / photo->initial_distance);
      photo->angle = photo->base_angle + (angle - photo->initial_angle);
      new_position = TRUE;
    }

  if (new_center)
    {
      gdouble origin_x, origin_y;

      origin_x = photo->x - photo->center_x;
      origin_y = photo->y - photo->center_y;

      calculate_rotated_point (- photo->angle,
                               1 / photo->zoom,
                               photo->x - origin_x,
                               photo->y - origin_y,
                               event_x - origin_x,
                               event_y - origin_y,
                               &photo->center_x,
                               &photo->center_y);
    }

  if (new_position)
    {
      photo->x = event_x;
      photo->y = event_y;
    }

  allocate_photo_region (photo);

  test_photo_bounding_rect (photo, &rect);
  cairo_region_union_rectangle (region, (cairo_rectangle_int_t *) &rect);

  gdk_window_invalidate_region (widget->window, region, FALSE);
}

static gboolean
test_photo_album_widget_expose (GtkWidget      *widget,
                                GdkEventExpose *event)
{
  TestPhotoAlbumWidgetPrivate *priv;
  cairo_t *cr;
  gint i;

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (widget);
  cr = gdk_cairo_create (widget->window);

  gdk_cairo_region (cr, event->region);
  cairo_clip (cr);

  for (i = 0; i < priv->photos->len; i++)
    {
      TestPhoto *photo = g_ptr_array_index (priv->photos, i);
      GdkRectangle rect;

      test_photo_bounding_rect (photo, &rect);

      if (!gdk_rectangle_intersect (&rect, &event->area, NULL))
        continue;

      cairo_save (cr);

      cairo_translate (cr, photo->points[0].x, photo->points[0].y);
      cairo_scale (cr, photo->zoom, photo->zoom);
      cairo_rotate (cr, photo->angle);

      cairo_set_source_surface (cr, photo->surface, 0, 0);
      cairo_paint (cr);

      cairo_restore (cr);
    }

  cairo_destroy (cr);

  return TRUE;
}

GtkWidget *
test_photo_album_widget_new (void)
{
  return g_object_new (TEST_TYPE_PHOTO_ALBUM_WIDGET, NULL);
}

void
test_photo_album_widget_add_image (TestPhotoAlbumWidget *album,
                                   GdkPixbuf            *pixbuf)
{
  TestPhotoAlbumWidgetPrivate *priv;
  TestPhoto *photo;

  g_return_if_fail (TEST_IS_PHOTO_ALBUM_WIDGET (album));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));

  priv = TEST_PHOTO_ALBUM_WIDGET_GET_PRIVATE (album);

  photo = test_photo_new (album, pixbuf);
  g_ptr_array_add (priv->photos, photo);

  if (gtk_widget_get_realized (GTK_WIDGET (album)) &&
      gtk_widget_is_drawable (GTK_WIDGET (album)))
    {
      GdkRectangle rect;

      test_photo_bounding_rect (photo, &rect);
      gdk_window_invalidate_rect (GTK_WIDGET (album)->window, &rect, FALSE);
    }
}
